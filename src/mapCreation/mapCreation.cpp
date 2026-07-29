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
    m_colSpace = static_cast<double>(m_canvasWidth) / m_cellsN;
    m_rowSpace = static_cast<double>(m_canvasHeight) / m_cellsN;
    m_cellSize = m_canvasWidth / m_cellsN;
    loadShapefile(shpPath);
    classifyCells();
    cleanupSeamGaps();
}

MapCreation MapCreation::fromCache(const std::string& cachePath) {
    MapCreation obj;
    obj.loadCache(cachePath);
    return obj;
}

MapCreation::MapCreation()
    : m_cellsN(0),
      m_canvasWidth(700),
      m_canvasHeight(700),
      m_colSpace(0.0),
      m_rowSpace(0.0),
      m_cellSize(0),
      m_minDepth(0.0),
      m_maxDepth(0.0)
{
}

MapCreation MapCreation::fromGridData(const std::vector<std::vector<int>>& grid,
                                       int cellsN, int canvasWidth, int canvasHeight) {
    MapCreation obj;
    obj.m_cellsN = cellsN;
    obj.m_canvasWidth = canvasWidth;
    obj.m_canvasHeight = canvasHeight;
    obj.m_colSpace = static_cast<double>(canvasWidth) / cellsN;
    obj.m_rowSpace = static_cast<double>(canvasHeight) / cellsN;
    obj.m_cellSize = canvasWidth / cellsN;
    obj.m_grid = grid;
    obj.m_minDepth = 0.0;
    obj.m_maxDepth = 0.0;
    std::cout << "Grid loaded from scenario data: " << cellsN << "x" << cellsN
              << ", " << obj.getWaterCount() << " water cells" << std::endl;
    return obj;
}

// ════════════════════════════════════════════════════════════════════════════════
//  SHAPEFILE LOADING
// ════════════════════════════════════════════════════════════════════════════════

void MapCreation::extractPolygon(OGRPolygon* ogrPoly,
                                  double depth1, double depth2,
                                  double minX, double maxY, double scale)
{
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
        double px = (geoX - minX) * scale;
        double py = (maxY - geoY) * scale;
        sp.vertices.push_back({px, py});
    }

    if (sp.vertices.size() >= 3) {
        m_polygons.push_back(std::move(sp));
    }
}

void MapCreation::loadShapefile(const std::string& shpPath) {
    if (shpPath.empty()) return;

    GDALAllRegister();

    GDALDataset* dataset = static_cast<GDALDataset*>(
        GDALOpenEx(shpPath.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr)
    );
    if (dataset == nullptr) {
        throw std::runtime_error("Failed to open shapefile: " + shpPath);
    }

    OGRLayer* layer = dataset->GetLayer(0);
    if (layer == nullptr) {
        GDALClose(dataset);
        throw std::runtime_error("No layers found in shapefile: " + shpPath);
    }

    OGREnvelope envelope;
    layer->GetExtent(&envelope);

    double minX = envelope.MinX, minY = envelope.MinY;
    double maxX = envelope.MaxX, maxY = envelope.MaxY;

    double geoWidth  = maxX - minX;
    double geoHeight = maxY - minY;
    double buffer = 20.0;
    double xScale = (m_canvasWidth - buffer) / geoWidth;
    double yScale = (m_canvasHeight - buffer) / geoHeight;
    double scale  = std::min(xScale, yScale);

    m_minDepth =  std::numeric_limits<double>::max();
    m_maxDepth = -std::numeric_limits<double>::max();

    layer->ResetReading();
    OGRFeature* feature = nullptr;

    while ((feature = layer->GetNextFeature()) != nullptr) {
        int drval2Idx = feature->GetFieldIndex("DRVAL2");
        if (drval2Idx >= 0) {
            double d2 = feature->GetFieldAsDouble(drval2Idx);
            m_minDepth = std::min(m_minDepth, d2);
            m_maxDepth = std::max(m_maxDepth, d2);
        }
        OGRFeature::DestroyFeature(feature);
    }

    layer->ResetReading();

    while ((feature = layer->GetNextFeature()) != nullptr) {
        double depth1 = 0.0, depth2 = 0.0;
        int d1Idx = feature->GetFieldIndex("DRVAL1");
        int d2Idx = feature->GetFieldIndex("DRVAL2");
        if (d1Idx >= 0) depth1 = feature->GetFieldAsDouble(d1Idx);
        if (d2Idx >= 0) depth2 = feature->GetFieldAsDouble(d2Idx);

        OGRGeometry* geom = feature->GetGeometryRef();
        if (geom == nullptr) { OGRFeature::DestroyFeature(feature); continue; }

        if (wkbFlatten(geom->getGeometryType()) == wkbPolygon) {
            extractPolygon(static_cast<OGRPolygon*>(geom), depth1, depth2, minX, maxY, scale);
        } else if (wkbFlatten(geom->getGeometryType()) == wkbMultiPolygon) {
            OGRMultiPolygon* multiPoly = static_cast<OGRMultiPolygon*>(geom);
            for (int i = 0; i < multiPoly->getNumGeometries(); i++) {
                extractPolygon(static_cast<OGRPolygon*>(multiPoly->getGeometryRef(i)),
                               depth1, depth2, minX, maxY, scale);
            }
        }
        OGRFeature::DestroyFeature(feature);
    }

    GDALClose(dataset);

    std::cout << "Shapefile loaded: " << m_polygons.size() << " polygons, "
              << "depth range [" << m_minDepth << ", " << m_maxDepth << "], "
              << "scale=" << scale << std::endl;
}

// ════════════════════════════════════════════════════════════════════════════════
//  POINT-IN-POLYGON
// ════════════════════════════════════════════════════════════════════════════════

bool MapCreation::pointInPolygon(double px, double py,
                                  const std::vector<std::pair<double, double>>& vertices)
{
    bool inside = false;
    int n = static_cast<int>(vertices.size());
    for (int i = 0, j = n - 1; i < n; j = i++) {
        double xi = vertices[i].first, yi = vertices[i].second;
        double xj = vertices[j].first, yj = vertices[j].second;
        if ((yi > py) != (yj > py) &&
            (px < (xj - xi) * (py - yi) / (yj - yi) + xi))
            inside = !inside;
    }
    return inside;
}

bool MapCreation::isPointInWater(double px, double py) const {
    for (const auto& poly : m_polygons) {
        if (pointInPolygon(px, py, poly.vertices)) return true;
    }
    return false;
}

// ════════════════════════════════════════════════════════════════════════════════
//  GRID CLASSIFICATION
// ════════════════════════════════════════════════════════════════════════════════

void MapCreation::classifyCells() {
    m_grid.resize(m_cellsN, std::vector<int>(m_cellsN, 1));
    int waterCount = 0, landCount = 0;
    double inset = 0.05;

    for (int row = 0; row < m_cellsN; row++) {
        for (int col = 0; col < m_cellsN; col++) {
            double posX = col * m_colSpace, posY = row * m_rowSpace;
            double x0 = posX + m_colSpace * inset, x1 = posX + m_colSpace * 0.5, x2 = posX + m_colSpace * (1.0 - inset);
            double y0 = posY + m_rowSpace * inset, y1 = posY + m_rowSpace * 0.5, y2 = posY + m_rowSpace * (1.0 - inset);

            double checkPoints[9][2] = {
                {x0,y0},{x1,y0},{x2,y0},{x0,y1},{x1,y1},{x2,y1},{x0,y2},{x1,y2},{x2,y2}
            };

            int waterHits = 0;
            for (int p = 0; p < 9; p++)
                if (isPointInWater(checkPoints[p][0], checkPoints[p][1])) waterHits++;

            if (waterHits >= 5) { m_grid[row][col] = 0; waterCount++; }
            else { m_grid[row][col] = 1; landCount++; }
        }
    }
    std::cout << "Grid classified: " << m_cellsN << "x" << m_cellsN
              << " = " << waterCount << " water, " << landCount << " land" << std::endl;
}

// ════════════════════════════════════════════════════════════════════════════════
//  SEAM GAP CLEANUP
// ════════════════════════════════════════════════════════════════════════════════

void MapCreation::cleanupSeamGaps() {
    const int maxPasses = 5, dr[8] = {-1,-1,-1,0,0,1,1,1}, dc[8] = {-1,0,1,-1,1,-1,0,1};
    int totalFixed = 0;

    for (int pass = 0; pass < maxPasses; pass++) {
        int fixedThisPass = 0;
        auto snap = m_grid;

        for (int row = 0; row < m_cellsN; row++) {
            for (int col = 0; col < m_cellsN; col++) {
                if (snap[row][col] != LAND) continue;
                int waterN = 0, totalN = 0;
                for (int d = 0; d < 8; d++) {
                    int nr = row + dr[d], nc = col + dc[d];
                    if (nr < 0 || nr >= m_cellsN || nc < 0 || nc >= m_cellsN) continue;
                    totalN++;
                    if (snap[nr][nc] == WATER) waterN++;
                }
                if (totalN >= 3 && waterN >= 6) { m_grid[row][col] = WATER; fixedThisPass++; continue; }
                if (waterN >= 3) {
                    double px = col * m_colSpace, py = row * m_rowSpace;
                    double os = 0.2, x0 = px - m_colSpace*os, y0 = py - m_rowSpace*os;
                    double sx = m_colSpace*(1+2*os), sy = m_rowSpace*(1+2*os);
                    bool found = false;
                    for (int sy2 = 0; sy2 < 5 && !found; sy2++)
                        for (int sx2 = 0; sx2 < 5 && !found; sx2++)
                            if (isPointInWater(x0+sx*(0.1+0.2*sx2), y0+sy*(0.1+0.2*sy2))) found = true;
                    if (found) { m_grid[row][col] = WATER; fixedThisPass++; }
                }
            }
        }
        totalFixed += fixedThisPass;
        if (fixedThisPass == 0) break;
        std::cout << "  Seam cleanup pass " << (pass+1) << ": fixed " << fixedThisPass << " cells\n";
    }
    if (totalFixed > 0) std::cout << "Seam cleanup: reclassified " << totalFixed << " land cells as water\n";
}

// ════════════════════════════════════════════════════════════════════════════════
//  GRID ACCESS
// ════════════════════════════════════════════════════════════════════════════════

const std::vector<std::vector<int>>& MapCreation::getGrid() const { return m_grid; }
int MapCreation::getCell(int row, int col) const { return isValid(row,col) ? m_grid[row][col] : -1; }
bool MapCreation::isValid(int row, int col) const { return row >= 0 && row < m_cellsN && col >= 0 && col < m_cellsN; }
bool MapCreation::isWater(int row, int col) const { return isValid(row,col) && m_grid[row][col] == WATER; }
bool MapCreation::isPassable(int row, int col) const { return isValid(row,col) && m_grid[row][col] != LAND; }

std::vector<std::pair<int, int>> MapCreation::getAllWaterCells() const {
    std::vector<std::pair<int, int>> waterCells;
    for (int row = 0; row < m_cellsN; row++)
        for (int col = 0; col < m_cellsN; col++)
            if (m_grid[row][col] == WATER) waterCells.push_back({col, row});
    return waterCells;
}

// ════════════════════════════════════════════════════════════════════════════════
//  UNIT PLACEMENT
// ════════════════════════════════════════════════════════════════════════════════

bool MapCreation::placeUnit(int row, int col, int unitType) {
    if (!isValid(row,col) || m_grid[row][col] != WATER) return false;
    m_grid[row][col] = unitType;
    return true;
}

bool MapCreation::removeUnit(int row, int col) {
    if (!isValid(row,col) || !isUnitCell(m_grid[row][col])) return false;
    m_grid[row][col] = WATER;
    return true;
}

void MapCreation::clearAllUnits() {
    for (int row = 0; row < m_cellsN; row++)
        for (int col = 0; col < m_cellsN; col++)
            if (isUnitCell(m_grid[row][col])) m_grid[row][col] = WATER;
}

int MapCreation::placeUnitsFromConfig(const std::vector<std::pair<std::string, std::pair<int,int>>>& units) {
    int placed = 0;
    for (const auto& [type, pos] : units) {
        int unitType = WATER;
        if      (type == "seeker")      unitType = SEEKER;
        else if (type == "target")      unitType = TARGET;
        else if (type == "detector")    unitType = DETECTOR;
        else if (type == "interceptor") unitType = INTERCEPTOR;
        else if (type == "attacker")    unitType = ATTACKER;
        else continue;
        if (placeUnit(pos.first, pos.second, unitType)) placed++;
    }
    std::cout << "Placed " << placed << " / " << units.size() << " units on grid\n";
    return placed;
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
    for (const auto& row : m_grid) for (int cell : row) if (cell == 0) count++;
    return count;
}

int MapCreation::getLandCount() const { return m_cellsN * m_cellsN - getWaterCount(); }

// ════════════════════════════════════════════════════════════════════════════════
//  CACHE I/O
// ════════════════════════════════════════════════════════════════════════════════

void MapCreation::saveCache(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) throw std::runtime_error("Cannot open cache file for writing: " + filepath);
    file << m_cellsN << " " << m_canvasWidth << " " << m_canvasHeight << " " << m_minDepth << " " << m_maxDepth << "\n";
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
    if (!file.is_open()) throw std::runtime_error("Cannot open cache file: " + cachePath);
    file >> m_cellsN >> m_canvasWidth >> m_canvasHeight >> m_minDepth >> m_maxDepth;
    m_colSpace = static_cast<double>(m_canvasWidth) / m_cellsN;
    m_rowSpace = static_cast<double>(m_canvasHeight) / m_cellsN;
    m_cellSize = m_canvasWidth / m_cellsN;
    m_grid.resize(m_cellsN, std::vector<int>(m_cellsN, 1));
    for (int row = 0; row < m_cellsN; row++)
        for (int col = 0; col < m_cellsN; col++)
            file >> m_grid[row][col];
    file.close();
    std::cout << "Grid loaded from cache: " << m_cellsN << "x" << m_cellsN
              << ", " << getWaterCount() << " water cells" << std::endl;
}

// ════════════════════════════════════════════════════════════════════════════════
//  DEBUG / DISPLAY
// ════════════════════════════════════════════════════════════════════════════════

void MapCreation::printGrid() const {
    std::cout << "\nGrid (" << m_cellsN << "x" << m_cellsN << "):\n\n";
    for (int row = 0; row < m_cellsN; row++) {
        for (int col = 0; col < m_cellsN; col++) {
            switch (m_grid[row][col]) {
                case WATER:       std::cout << '.'; break;
                case LAND:        std::cout << '#'; break;
                case SEEKER:      std::cout << 'S'; break;
                case TARGET:      std::cout << 'T'; break;
                case DETECTOR:    std::cout << 'D'; break;
                case INTERCEPTOR: std::cout << 'I'; break;
                case ATTACKER:    std::cout << 'A'; break;
                default:          std::cout << '?'; break;
            }
        }
        std::cout << '\n';
    }
    std::cout << std::endl;
}

void MapCreation::printStats() const {
    int water = getWaterCount(), total = m_cellsN * m_cellsN, land = total - water;
    std::cout << "\n=== Map Statistics ===" << std::endl;
    std::cout << "  Grid size:    " << m_cellsN << " x " << m_cellsN << std::endl;
    std::cout << "  Cell size:    " << m_cellSize << " px" << std::endl;
    std::cout << "  Total cells:  " << total << std::endl;
    std::cout << "  Water cells:  " << water << " (" << (100.0*water/total) << "%)" << std::endl;
    std::cout << "  Land cells:   " << land << " (" << (100.0*land/total) << "%)" << std::endl;
    std::cout << "  Depth range:  [" << m_minDepth << ", " << m_maxDepth << "]" << std::endl;
    std::cout << "=====================\n" << std::endl;
}
