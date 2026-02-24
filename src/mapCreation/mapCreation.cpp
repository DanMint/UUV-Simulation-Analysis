#include "mapCreation.h"

// GDAL/OGR headers for reading shapefiles
#include <ogrsf_frmts.h>

#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <limits>
#include <stdexcept>

// ════════════════════════════════════════════════════════════════════════════════
//  CONSTRUCTORS
// ════════════════════════════════════════════════════════════════════════════════

MapCreation::MapCreation(const std::string& shpPath, int cellsN,
                         int canvasWidth, int canvasHeight)
    : m_cellsN(cellsN),
      m_canvasWidth(canvasWidth),
      m_canvasHeight(canvasHeight),
      m_minDepth(0.0),
      m_maxDepth(0.0)
{
    // Calculate cell spacing (same math as the Python version)
    m_colSpace = static_cast<double>(m_canvasWidth) / m_cellsN;
    m_rowSpace = static_cast<double>(m_canvasHeight) / m_cellsN;
    m_cellSize = m_canvasWidth / m_cellsN;

    // Step 1: Read the shapefile and build scaled polygons
    loadShapefile(shpPath);

    // Step 2: Walk the grid and classify each cell as water or land
    classifyCells();
}

MapCreation MapCreation::fromCache(const std::string& cachePath) {
    // Create a "blank" object then fill it from cache
    // We use a private helper, so we construct with dummy values
    // and immediately overwrite from cache
    MapCreation obj("", 0, 0, 0);  // won't actually load anything since cellsN=0
    // Clear the failed state from the dummy constructor
    obj.m_grid.clear();
    obj.m_polygons.clear();
    obj.loadCache(cachePath);
    return obj;
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

    // ── Get the first (and usually only) layer ──
    OGRLayer* layer = dataset->GetLayer(0);
    if (layer == nullptr) {
        GDALClose(dataset);
        throw std::runtime_error("No layers found in shapefile: " + shpPath);
    }

    // ── First pass: find geographic bounding box and depth range ──
    OGREnvelope envelope;
    layer->GetExtent(&envelope);

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

/**
 * Classic ray casting algorithm:
 * Cast a horizontal ray from the point to the right.
 * Count how many polygon edges it crosses.
 * If odd → inside. If even → outside.
 */
bool MapCreation::pointInPolygon(double px, double py,
                                  const std::vector<std::pair<double, double>>& vertices)
{
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
        if ((yi > py) != (yj > py) &&
            (px < (xj - xi) * (py - yi) / (yj - yi) + xi))
        {
            inside = !inside;  // flip: each crossing toggles inside/outside
        }
    }
    return inside;
}

bool MapCreation::isPointInWater(double px, double py) const {
    // Check against every water polygon — if point is inside any, it's water
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
    // Allocate the grid: m_cellsN rows, each with m_cellsN columns
    m_grid.resize(m_cellsN, std::vector<int>(m_cellsN, 1));  // default to land (1)

    int waterCount = 0;
    int landCount  = 0;

    for (int row = 0; row < m_cellsN; row++) {
        for (int col = 0; col < m_cellsN; col++) {
            // Calculate the top-left pixel position of this cell
            double posX = col * m_colSpace;
            double posY = row * m_rowSpace;

            // 5-point sampling: four corners + center
            // Same logic as the Python Grid.draw_test_grid()
            double checkPoints[5][2] = {
                {posX,                     posY},                       // top-left
                {posX + m_colSpace,        posY},                       // top-right
                {posX,                     posY + m_rowSpace},          // bottom-left
                {posX + m_colSpace,        posY + m_rowSpace},          // bottom-right
                {posX + m_colSpace / 2.0,  posY + m_rowSpace / 2.0},   // center
            };

            // Count how many of the 5 points are in water
            int waterHits = 0;
            for (int p = 0; p < 5; p++) {
                if (isPointInWater(checkPoints[p][0], checkPoints[p][1])) {
                    waterHits++;
                }
            }

            // If 3 or more points are water → cell is water (0)
            if (waterHits >= 3) {
                m_grid[row][col] = 0;
                waterCount++;
            } else {
                m_grid[row][col] = 1;
                landCount++;
            }
        }
    }

    std::cout << "Grid classified: " << m_cellsN << "x" << m_cellsN
              << " = " << waterCount << " water, " << landCount << " land"
              << std::endl;
}

// ════════════════════════════════════════════════════════════════════════════════
//  GRID ACCESS
// ════════════════════════════════════════════════════════════════════════════════

const std::vector<std::vector<int>>& MapCreation::getGrid() const {
    return m_grid;
}

int MapCreation::getCell(int row, int col) const {
    if (!isValid(row, col)) return -1;
    return m_grid[row][col];
}

bool MapCreation::isValid(int row, int col) const {
    return row >= 0 && row < m_cellsN && col >= 0 && col < m_cellsN;
}

bool MapCreation::isWater(int row, int col) const {
    return isValid(row, col) && m_grid[row][col] == 0;
}

std::vector<std::pair<int, int>> MapCreation::getAllWaterCells() const {
    std::vector<std::pair<int, int>> waterCells;
    for (int row = 0; row < m_cellsN; row++) {
        for (int col = 0; col < m_cellsN; col++) {
            if (m_grid[row][col] == 0) {
                waterCells.push_back({col, row});  // (col, row) to match Python convention
            }
        }
    }
    return waterCells;
}

// ════════════════════════════════════════════════════════════════════════════════
//  GRID INFO
// ════════════════════════════════════════════════════════════════════════════════

int MapCreation::getCellsN() const       { return m_cellsN; }
int MapCreation::getCanvasWidth() const  { return m_canvasWidth; }
int MapCreation::getCanvasHeight() const { return m_canvasHeight; }
int MapCreation::getCellSize() const     { return m_cellSize; }
double MapCreation::getMinDepth() const  { return m_minDepth; }
double MapCreation::getMaxDepth() const  { return m_maxDepth; }

int MapCreation::getWaterCount() const {
    int count = 0;
    for (const auto& row : m_grid)
        for (int cell : row)
            if (cell == 0) count++;
    return count;
}

int MapCreation::getLandCount() const {
    int total = m_cellsN * m_cellsN;
    return total - getWaterCount();
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
    file << m_cellsN << " " << m_canvasWidth << " " << m_canvasHeight
         << " " << m_minDepth << " " << m_maxDepth << "\n";

    // Grid data: one row per line, space-separated 0s and 1s
    for (int row = 0; row < m_cellsN; row++) {
        for (int col = 0; col < m_cellsN; col++) {
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

    // Read header
    file >> m_cellsN >> m_canvasWidth >> m_canvasHeight >> m_minDepth >> m_maxDepth;

    // Recalculate spacing
    m_colSpace = static_cast<double>(m_canvasWidth) / m_cellsN;
    m_rowSpace = static_cast<double>(m_canvasHeight) / m_cellsN;
    m_cellSize = m_canvasWidth / m_cellsN;

    // Read grid
    m_grid.resize(m_cellsN, std::vector<int>(m_cellsN, 1));
    for (int row = 0; row < m_cellsN; row++) {
        for (int col = 0; col < m_cellsN; col++) {
            file >> m_grid[row][col];
        }
    }

    file.close();

    int water = getWaterCount();
    std::cout << "Grid loaded from cache: " << m_cellsN << "x" << m_cellsN
              << ", " << water << " water cells" << std::endl;
}

// ════════════════════════════════════════════════════════════════════════════════
//  DEBUG / DISPLAY
// ════════════════════════════════════════════════════════════════════════════════

void MapCreation::printGrid() const {
    std::cout << "\nGrid (" << m_cellsN << "x" << m_cellsN
              << ", . = water, # = land):\n\n";
    for (int row = 0; row < m_cellsN; row++) {
        for (int col = 0; col < m_cellsN; col++) {
            std::cout << (m_grid[row][col] == 0 ? '.' : '#');
        }
        std::cout << '\n';
    }
    std::cout << std::endl;
}

void MapCreation::printStats() const {
    int water = getWaterCount();
    int total = m_cellsN * m_cellsN;
    int land  = total - water;

    std::cout << "\n=== Map Statistics ===" << std::endl;
    std::cout << "  Grid size:    " << m_cellsN << " x " << m_cellsN << std::endl;
    std::cout << "  Cell size:    " << m_cellSize << " px" << std::endl;
    std::cout << "  Total cells:  " << total << std::endl;
    std::cout << "  Water cells:  " << water
              << " (" << (100.0 * water / total) << "%)" << std::endl;
    std::cout << "  Land cells:   " << land
              << " (" << (100.0 * land / total) << "%)" << std::endl;
    std::cout << "  Depth range:  [" << m_minDepth << ", " << m_maxDepth << "]" << std::endl;
    std::cout << "=====================\n" << std::endl;
}