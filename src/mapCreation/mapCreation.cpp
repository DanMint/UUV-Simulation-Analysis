#include "mapCreation.h"
#include "spawnConfig.h"

// GDAL = translator library for raster and vector geospatial data formats

// GDAL/OGR headers for reading shapefiles
#include <ogrsf_frmts.h>

#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <limits>
#include <stdexcept>


/**
 * Default constructor that reads a .shp file and translates it to a grid (std::vector<std::vector<int>> m_grid)
 * 
 * @param shpPath - path to the .shp file
 * @param cellsN - number of cells inside the 
 * @param canvasWidth
 * @param canvasHeight 
 * 
 */
MapCreation::MapCreation(const std::string& shpPath,
                         int cellsInARow,
                         int canvasWidth,
                         int canvasHeight)
    : m_cellsInARow(cellsInARow),
      m_canvasWidth(canvasWidth),
      m_canvasHeight(canvasHeight)
{
    if (m_cellsInARow <= 0) {
        throw std::invalid_argument("cellsInARow must be greater than zero");
    }
    if (m_canvasWidth <= 0 || m_canvasHeight <= 0) {
        throw std::invalid_argument("canvas dimensions must be greater than zero");
    }

    // Cell spacing calculation.
    m_colSpace = static_cast<double>(m_canvasWidth) / m_cellsInARow;
    m_rowSpace = static_cast<double>(m_canvasHeight) / m_cellsInARow;
    m_cellSize = m_canvasWidth / m_cellsInARow;

    // Step 1: Read the shapefile and build scaled polygons.
    loadShapefile(shpPath);

    // Step 2: Walk the grid and classify each cell as water or land.
    classifyCells();

    // Step 3: Fix polygon seam gaps (thin land lines through water).
    cleanupSeamGaps();
}

MapCreation MapCreation::fromCache(const std::string& cachePath) {
    // Use the private blank constructor so cache loading does not need dummy
    // dimensions and never performs division by zero.
    MapCreation map;
    map.loadCache(cachePath);
    return map;
}

// ════════════════════════════════════════════════════════════════════════════════
//  SHAPEFILE LOADING
// ════════════════════════════════════════════════════════════════════════════════

/**
 * Helper: extract exterior ring coordinates from a single OGRPolygon
 * and push a ScaledPolygon onto m_polygons.
 *
 * Scaling formula (matches the Python MapControl):
 *   pixel_x = (geo_x - minX) * scale
 *   pixel_y = (maxY - geo_y) * scale
 *
 * The Y-axis is flipped because geographic Y increases upward
 * but pixel Y increases downward.
 */
void MapCreation::extractPolygon(OGRPolygon* ogrPoly,
                                  double depth1, double depth2,
                                  double minX, double maxY, double scale)
{
    // Get the exterior ring (the outer boundary of the polygon)
    OGRLinearRing* ring = ogrPoly->getExteriorRing();
    if (ring == nullptr) return;

    ScaledPolygon sp;
    sp.depth1 = depth1;
    sp.depth2 = depth2;

    int numPoints = ring->getNumPoints();
    sp.vertices.reserve(numPoints);

    for (int i = 0; i < numPoints; i++) {
        double geoX = ring->getX(i);
        double geoY = ring->getY(i);

        // Scale to pixel space (same formula as Python)
        double px = (geoX - minX) * scale;
        double py = (maxY - geoY) * scale;

        sp.vertices.push_back({px, py});
    }

    // Only keep non-degenerate polygons (need at least 3 vertices)
    if (sp.vertices.size() >= 3) {
        m_polygons.push_back(std::move(sp));
    }
}

void MapCreation::loadShapefile(const std::string& shpPath) {
    // Skip loading if path is empty (used by fromCache factory)
    if (shpPath.empty()) return;

    // ── Initialize GDAL (safe to call multiple times) ──
    GDALAllRegister();

    // ── Open the shapefile ──
    GDALDataset* dataset = static_cast<GDALDataset*>(
        GDALOpenEx(shpPath.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr)
    );
    if (dataset == nullptr) {
        throw std::runtime_error("Failed to open shapefile: " + shpPath);
    }

    // Get the first (and usually only) layer
    OGRLayer* layer = dataset->GetLayer(0);
    if (layer == nullptr) {
        GDALClose(dataset);
        throw std::runtime_error("No layers found in shapefile: " + shpPath);
    }

    // First pass: find geographic bounding box and depth range
    OGREnvelope envelope;
    if (layer->GetExtent(&envelope, TRUE) != OGRERR_NONE) {
        GDALClose(dataset);
        throw std::runtime_error(
            "Failed to read shapefile extent: " + shpPath);
    }

    double minX = envelope.MinX;
    double minY = envelope.MinY;
    double maxX = envelope.MaxX;
    double maxY = envelope.MaxY;

    double geoWidth  = maxX - minX;
    double geoHeight = maxY - minY;

    // Calculate scale factor (same as Python: buffer=20)
    double buffer = 20.0;
    double xScale = (m_canvasWidth - buffer) / geoWidth;
    double yScale = (m_canvasHeight - buffer) / geoHeight;
    double scale  = std::min(xScale, yScale);

    // Find depth range
    m_minDepth =  std::numeric_limits<double>::max();
    m_maxDepth = -std::numeric_limits<double>::max();

    layer->ResetReading();
    OGRFeature* feature = nullptr;

    // loop goes through every GDAL/OGR layer, reads the DRYVAL2 and updated min and max depth values
    while ((feature = layer->GetNextFeature()) != nullptr) {
        // Read depth columns
        int drval2Idx = feature->GetFieldIndex("DRVAL2");
        if (drval2Idx >= 0) {
            double d2 = feature->GetFieldAsDouble(drval2Idx);
            m_minDepth = std::min(m_minDepth, d2);
            m_maxDepth = std::max(m_maxDepth, d2);
        }
        OGRFeature::DestroyFeature(feature);
    }

    // ── Second pass: extract and scale polygon geometry ──
    layer->ResetReading();

    while ((feature = layer->GetNextFeature()) != nullptr) {
        // Read depth values
        double depth1 = 0.0, depth2 = 0.0;
        int d1Idx = feature->GetFieldIndex("DRVAL1");
        int d2Idx = feature->GetFieldIndex("DRVAL2");
        if (d1Idx >= 0) depth1 = feature->GetFieldAsDouble(d1Idx);
        if (d2Idx >= 0) depth2 = feature->GetFieldAsDouble(d2Idx);

        // Get geometry
        OGRGeometry* geom = feature->GetGeometryRef();
        if (geom == nullptr) {
            OGRFeature::DestroyFeature(feature);
            continue;
        }

        // Handle Polygon
        if (wkbFlatten(geom->getGeometryType()) == wkbPolygon) {
            OGRPolygon* poly = static_cast<OGRPolygon*>(geom);
            extractPolygon(poly, depth1, depth2, minX, maxY, scale);
        }
        // Handle MultiPolygon (contains multiple polygons)
        else if (wkbFlatten(geom->getGeometryType()) == wkbMultiPolygon) {
            OGRMultiPolygon* multiPoly = static_cast<OGRMultiPolygon*>(geom);
            for (int i = 0; i < multiPoly->getNumGeometries(); i++) {
                OGRPolygon* poly = static_cast<OGRPolygon*>(multiPoly->getGeometryRef(i));
                extractPolygon(poly, depth1, depth2, minX, maxY, scale);
            }
        }

        OGRFeature::DestroyFeature(feature);
    }

    // ── Cleanup ──
    GDALClose(dataset);

    std::cout << "Shapefile loaded: " << m_polygons.size() << " polygons, "
              << "depth range [" << m_minDepth << ", " << m_maxDepth << "], "
              << "scale=" << scale << std::endl;
}

// ════════════════════════════════════════════════════════════════════════════════
//  POINT-IN-POLYGON (Ray Casting Algorithm)
// ════════════════════════════════════════════════════════════════════════════════

bool MapCreation::pointInPolygon(double px, double py, const std::vector<std::pair<double, double>>& vertices) {
    bool inside = false;
    int n = static_cast<int>(vertices.size());

    // j starts at the last vertex, i starts at the first
    // This lets us check each edge (vertices[j] → vertices[i])
    for (int i = 0, j = n - 1; i < n; j = i++) {
        double xi = vertices[i].first;
        double yi = vertices[i].second;
        double xj = vertices[j].first;
        double yj = vertices[j].second;

        // Check if the ray crosses this edge
        // Condition 1: the point's Y is between the edge's Y range
        // Condition 2: the point's X is to the left of the edge intersection
        if ((yi > py) != (yj > py) && (px < (xj - xi) * (py - yi) / (yj - yi) + xi)) {
            // flip each crossing toggles inside/outside
            inside = !inside;  
        }
    }
    return inside;
}

bool MapCreation::isPointInWater(double px, double py) const {
    // Check against every water polygon. if point is inside any its water
    for (const auto& poly : m_polygons) {
        if (pointInPolygon(px, py, poly.vertices)) {
            return true;
        }
    }
    return false;
}

// ════════════════════════════════════════════════════════════════════════════════
//  GRID CLASSIFICATION
// ════════════════════════════════════════════════════════════════════════════════

void MapCreation::classifyCells() {
    // Allocate the grid: m_cellsInARow rows, each with m_cellsInARow columns
    m_grid.resize(m_cellsInARow, std::vector<int>(m_cellsInARow, 1));  // default to land (1)

    int waterCount = 0;
    int landCount  = 0;

    // Small inset to avoid testing exactly on polygon edges where
    // floating-point ray casting is unreliable
    double inset = 0.05;

    for (int row = 0; row < m_cellsInARow; row++) {
        for (int col = 0; col < m_cellsInARow; col++) {
            // Calculate the top-left pixel position of this cell
            double posX = col * m_colSpace;
            double posY = row * m_rowSpace;

            // 9-point sampling: 3x3 grid with inset from cell edges
            // This catches seam gaps much better than 5-point corner sampling
            // because interior points are less likely to land exactly on a
            // polygon boundary where ray casting is ambiguous.
            double x0 = posX + m_colSpace * inset;
            double x1 = posX + m_colSpace * 0.5;
            double x2 = posX + m_colSpace * (1.0 - inset);
            double y0 = posY + m_rowSpace * inset;
            double y1 = posY + m_rowSpace * 0.5;
            double y2 = posY + m_rowSpace * (1.0 - inset);

            double checkPoints[9][2] = {
                {x0, y0}, {x1, y0}, {x2, y0},   // top row
                {x0, y1}, {x1, y1}, {x2, y1},   // middle row
                {x0, y2}, {x1, y2}, {x2, y2},   // bottom row
            };

            // Count how many of the 9 points are in water
            int waterHits = 0;
            for (int p = 0; p < 9; p++) {
                if (isPointInWater(checkPoints[p][0], checkPoints[p][1])) {
                    waterHits++;
                }
            }

            // If 5 or more points (majority) are water → cell is water
            if (waterHits >= 5) {
                m_grid[row][col] = 0;
                waterCount++;
            } else {
                m_grid[row][col] = 1;
                landCount++;
            }
        }
    }

    std::cout << "Grid classified: " << m_cellsInARow << "x" << m_cellsInARow
              << " = " << waterCount << " water, " << landCount << " land"
              << std::endl;
}

// ════════════════════════════════════════════════════════════════════════════════
//  SEAM GAP CLEANUP
// ════════════════════════════════════════════════════════════════════════════════

//  After initial classification, thin lines of land can appear where
//  adjacent polygons in the shapefile don't perfectly overlap (seam gaps).
//
//  Two strategies:
//
//  1. Neighbor threshold: land cell with 6+ water neighbors out of 8
//     → flip unconditionally. Safe — these are isolated dots.
//
//  2. Dense polygon re-check: for any land cell with 3+ water neighbors,
//     re-test with a 5×5 grid of 25 sample points that extend 20% beyond
//     the cell boundary on each side. If ANY point hits a water polygon
//     → flip. The overshoot lets sample points reach into adjacent water
//     polygons even when the seam gap is wider than one cell.
//
//     This is safe for real land because: on a genuine land cell,
//     none of the 25 points will be inside any water polygon — the
//     polygons simply don't cover that area. Only seam cells (which
//     are geometrically surrounded by water polygons with a thin gap)
//     will have points that clip into a polygon edge.
//
//  Runs iteratively until convergence (max 5 passes).
//
// ════════════════════════════════════════════════════════════════════════════════

void MapCreation::cleanupSeamGaps() {
    const int maxPasses = 5;
    int totalFixed = 0;

    // 8-directional neighbor offsets
    const int dr[8] = { -1, -1, -1,  0, 0,  1, 1, 1 };
    const int dc[8] = { -1,  0,  1, -1, 1, -1, 0, 1 };

    for (int pass = 0; pass < maxPasses; pass++) {
        int fixedThisPass = 0;

        // Snapshot so reads and writes don't interfere within a pass
        std::vector<std::vector<int>> snap = m_grid;

        for (int row = 0; row < m_cellsInARow; row++) {
            for (int col = 0; col < m_cellsInARow; col++) {
                if (snap[row][col] != LAND) continue;

                // Count water neighbors
                int waterNeighbors = 0;
                int totalNeighbors = 0;
                for (int d = 0; d < 8; d++) {
                    int nr = row + dr[d];
                    int nc = col + dc[d];
                    if (nr < 0 || nr >= m_cellsInARow || nc < 0 || nc >= m_cellsInARow)
                        continue;
                    totalNeighbors++;
                    if (snap[nr][nc] == WATER)
                        waterNeighbors++;
                }

                // ── Strategy 1: obvious isolated land cells ──
                if (totalNeighbors >= 3 && waterNeighbors >= 6) {
                    m_grid[row][col] = WATER;
                    fixedThisPass++;
                    continue;
                }

                // ── Strategy 2: dense polygon re-check ──
                // For borderline cells (3+ water neighbors), test a 5×5
                // grid of 25 points that extends 20% beyond the cell
                // boundary. The overshoot lets sample points reach into
                // adjacent water polygons even when the seam gap is
                // wider than the cell itself.
                if (waterNeighbors >= 3) {
                    double posX = col * m_colSpace;
                    double posY = row * m_rowSpace;

                    // Extend 20% beyond cell boundary on each side
                    double overshoot = 0.2;
                    double x0 = posX - m_colSpace * overshoot;
                    double y0 = posY - m_rowSpace * overshoot;
                    double spanX = m_colSpace * (1.0 + 2.0 * overshoot);
                    double spanY = m_rowSpace * (1.0 + 2.0 * overshoot);

                    bool foundWater = false;
                    // 5×5 grid across the extended area
                    for (int sy = 0; sy < 5 && !foundWater; sy++) {
                        for (int sx = 0; sx < 5 && !foundWater; sx++) {
                            double px = x0 + spanX * (0.1 + 0.2 * sx);
                            double py = y0 + spanY * (0.1 + 0.2 * sy);
                            if (isPointInWater(px, py)) {
                                foundWater = true;
                            }
                        }
                    }

                    if (foundWater) {
                        m_grid[row][col] = WATER;
                        fixedThisPass++;
                    }
                }
            }
        }

        totalFixed += fixedThisPass;

        if (fixedThisPass == 0) break;  // converged

        std::cout << "  Seam cleanup pass " << (pass + 1)
                  << ": fixed " << fixedThisPass << " cells\n";
    }

    if (totalFixed > 0) {
        std::cout << "Seam cleanup: reclassified " << totalFixed
                  << " land cells as water\n";
    }
}

// ════════════════════════════════════════════════════════════════════════════════
//  GRID ACCESS
// ════════════════════════════════════════════════════════════════════════════════

const std::vector<std::vector<int>>& MapCreation::getGrid() const {
    return m_grid;
}

int MapCreation::getCell(int row, int col) const {
    if (!isValid(row, col)) 
        return -1;

    return m_grid[row][col];
}

bool MapCreation::isValid(int row, int col) const {
    return row >= 0 && row < m_cellsInARow && col >= 0 && col < m_cellsInARow;
}

bool MapCreation::isWater(int row, int col) const {
    return isValid(row, col) && m_grid[row][col] == WATER;
}

bool MapCreation::isPassable(int row, int col) const {
    // Every non-land cell is traversable at the map layer. Higher-level
    // collision rules can still reject cells occupied by particular units.
    return isValid(row, col) && m_grid[row][col] != LAND;
}

std::vector<std::pair<int, int>> MapCreation::getAllWaterCells() const {
    std::vector<std::pair<int, int>> waterCells;
    for (int row = 0; row < m_cellsInARow; row++) {
        for (int col = 0; col < m_cellsInARow; col++) {
            if (m_grid[row][col] == WATER) {
                waterCells.push_back({col, row});
            }
        }
    }
    return waterCells;
}

// ════════════════════════════════════════════════════════════════════════════════
//  UNIT PLACEMENT ON GRID
// ════════════════════════════════════════════════════════════════════════════════

int MapCreation::categoryToCellType(const std::string& category) {
    if (category == "seeker") {
        return SEEKER;
    }
    if (category == "target") {
        return TARGET;
    }
    if (category == "detector") {
        return DETECTOR;
    }
    if (category == "interceptor") {
        return INTERCEPTOR;
    }

    throw std::invalid_argument("Unknown unit category: " + category);
}

bool MapCreation::placeUnit(int row, int col, int unitType) {
    if (!isValid(row, col)) {
        return false;
    }

    // Reject terrain values and arbitrary integers. A concrete type such as
    // "basic" is not encoded here; only the broad category is stored.
    if (!isUnitCell(unitType)) {
        return false;
    }

    // Units may only be placed on unoccupied water cells.
    if (m_grid[row][col] != WATER) {
        return false;
    }

    m_grid[row][col] = unitType;
    return true;
}

bool MapCreation::removeUnit(int row, int col) {
    if (!isValid(row, col)) {
        return false;
    }

    if (isUnitCell(m_grid[row][col])) {
        m_grid[row][col] = WATER;
        return true;
    }

    return false;
}

void MapCreation::clearAllUnits() {
    for (int row = 0; row < m_cellsInARow; row++) {
        for (int col = 0; col < m_cellsInARow; col++) {
            if (isUnitCell(m_grid[row][col])) {
                m_grid[row][col] = WATER;
            }
        }
    }
}

int MapCreation::placeUnitsFromConfig(const std::vector<UnitSpawn>& units) {
    int placed = 0;

    for (const auto& unit : units) {
        try {
            const int cellType = categoryToCellType(unit.category);

            if (placeUnit(unit.row, unit.col, cellType)) {
                ++placed;
            }
        }
        catch (const std::invalid_argument& error) {
            std::cerr << "Skipping unit at (" << unit.row << ", " << unit.col
                      << "): " << error.what() << '\n';
        }
    }

    std::cout << "Placed " << placed << " / " << units.size()
              << " configured units on grid\n";
    return placed;
}

int MapCreation::placeUnitsFromConfig(
    const std::vector<std::pair<std::string, std::pair<int, int>>>& units)
{
    int placed = 0;

    for (const auto& [category, position] : units) {
        try {
            const int cellType = categoryToCellType(category);

            if (placeUnit(position.first, position.second, cellType)) {
                ++placed;
            }
        }
        catch (const std::invalid_argument& error) {
            std::cerr << "Skipping legacy unit at ("
                      << position.first << ", " << position.second
                      << "): " << error.what() << '\n';
        }
    }

    std::cout << "Placed " << placed << " / " << units.size()
              << " legacy configured units on grid\n";
    return placed;
}

// ════════════════════════════════════════════════════════════════════════════════
//  GRID INFO
// ════════════════════════════════════════════════════════════════════════════════

int MapCreation::getCellsN() const { 
    return m_cellsInARow; 
}

int MapCreation::getCanvasWidth() const {
     return m_canvasWidth; 
}

int MapCreation::getCanvasHeight() const { 
    return m_canvasHeight; 
}

int MapCreation::getCellSize() const { 
    return m_cellSize; 
}

double MapCreation::getMinDepth() const { 
    return m_minDepth; 
}

double MapCreation::getMaxDepth() const { 
    return m_maxDepth; 
}

int MapCreation::getWaterCount() const {
    int count = 0;

    for (const auto& row : m_grid) {
        for (int cell : row) {
            if (cell == WATER) {
                ++count;
            }
        }
    }

    return count;
}

int MapCreation::getLandCount() const {
    int count = 0;

    for (const auto& row : m_grid) {
        for (int cell : row) {
            if (cell == LAND) {
                ++count;
            }
        }
    }

    return count;
}

// ════════════════════════════════════════════════════════════════════════════════
//  CACHE I/O
// ════════════════════════════════════════════════════════════════════════════════

void MapCreation::saveCache(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open cache file for writing: " + filepath);
    }

    // Header line: cellsN canvasWidth canvasHeight minDepth maxDepth
    file << m_cellsInARow << " " << m_canvasWidth << " " << m_canvasHeight
         << " " << m_minDepth << " " << m_maxDepth << "\n";

    // Grid data: one row per line, space-separated 0s and 1s
    for (int row = 0; row < m_cellsInARow; row++) {
        for (int col = 0; col < m_cellsInARow; col++) {
            if (col > 0) file << " ";
            file << m_grid[row][col];
        }
        file << "\n";
    }

    file.close();
    std::cout << "Grid cache saved to " << filepath << std::endl;
}

void MapCreation::loadCache(const std::string& cachePath) {
    std::ifstream file(cachePath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open cache file: " + cachePath);
    }

    // Read and validate the header before performing spacing calculations.
    if (!(file >> m_cellsInARow
               >> m_canvasWidth
               >> m_canvasHeight
               >> m_minDepth
               >> m_maxDepth)) {
        throw std::runtime_error("Invalid cache header: " + cachePath);
    }

    if (m_cellsInARow <= 0 || m_canvasWidth <= 0 || m_canvasHeight <= 0) {
        throw std::runtime_error("Invalid cache dimensions: " + cachePath);
    }

    m_colSpace = static_cast<double>(m_canvasWidth) / m_cellsInARow;
    m_rowSpace = static_cast<double>(m_canvasHeight) / m_cellsInARow;
    m_cellSize = m_canvasWidth / m_cellsInARow;

    m_grid.assign(m_cellsInARow, std::vector<int>(m_cellsInARow, LAND));

    for (int row = 0; row < m_cellsInARow; ++row) {
        for (int col = 0; col < m_cellsInARow; ++col) {
            if (!(file >> m_grid[row][col])) {
                throw std::runtime_error("Incomplete cache grid: " + cachePath);
            }
        }
    }

    const int water = getWaterCount();
    std::cout << "Grid loaded from cache: " << m_cellsInARow << "x"
              << m_cellsInARow << ", " << water << " water cells"
              << std::endl;
}

// ════════════════════════════════════════════════════════════════════════════════
//  DEBUG / DISPLAY
// ════════════════════════════════════════════════════════════════════════════════

void MapCreation::printGrid() const {
    std::cout << "\nGrid (" << m_cellsInARow << "x" << m_cellsInARow
              << ", . = water, # = land, S = seeker, T = target, "
              << "D = detector, I = interceptor):\n\n";
    for (int row = 0; row < m_cellsInARow; row++) {
        for (int col = 0; col < m_cellsInARow; col++) {
            switch (m_grid[row][col]) {
                case WATER:       
                    std::cout << '.'; break;

                case LAND:        
                    std::cout << '#'; break;

                case SEEKER:      
                    std::cout << 'S'; break;

                case TARGET:      
                    std::cout << 'T'; break;

                case DETECTOR:      
                    std::cout << 'D'; break;

                case INTERCEPTOR: 
                    std::cout << 'I'; break;

                default:          
                    std::cout << '?'; break;
            }
        }
        std::cout << '\n';
    }
    std::cout << std::endl;
}

void MapCreation::printStats() const {
    int water = getWaterCount();
    int total = m_cellsInARow * m_cellsInARow;
    int land  = total - water;

    std::cout << "\n=== Map Statistics ===" << std::endl;
    std::cout << "  Grid size:    " << m_cellsInARow << " x " << m_cellsInARow << std::endl;
    std::cout << "  Cell size:    " << m_cellSize << " px" << std::endl;
    std::cout << "  Total cells:  " << total << std::endl;
    std::cout << "  Water cells:  " << water
              << " (" << (100.0 * water / total) << "%)" << std::endl;
    std::cout << "  Land cells:   " << land
              << " (" << (100.0 * land / total) << "%)" << std::endl;
    std::cout << "  Depth range:  [" << m_minDepth << ", " << m_maxDepth << "]" << std::endl;
    std::cout << "=====================\n" << std::endl;
}

// ════════════════════════════════════════════════════════════════════════════════
//  HELPER 
// ════════════════════════════════════════════════════════════════════════════════

bool MapCreation::isUnitCell(int v) {
    return v == SEEKER || v == TARGET || v == DETECTOR || v == INTERCEPTOR;

}