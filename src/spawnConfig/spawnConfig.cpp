#include "spawnConfig.h"
#include <algorithm>

// ════════════════════════════════════════════════════════════════════════════════
//  UNIT MANAGEMENT
// ════════════════════════════════════════════════════════════════════════════════

bool SpawnConfig::addUnit(const std::string& type, int row, int col) {
    for (const auto& u : m_units) {
        if (u.row == row && u.col == col) return false;
    }
    m_units.push_back({type, row, col});
    return true;
}

bool SpawnConfig::removeUnit(int row, int col) {
    for (auto it = m_units.begin(); it != m_units.end(); ++it) {
        if (it->row == row && it->col == col) {
            m_units.erase(it);
            return true;
        }
    }
    return false;
}

const UnitSpawn* SpawnConfig::getUnitAt(int row, int col) const {
    for (const auto& u : m_units) {
        if (u.row == row && u.col == col) return &u;
    }
    return nullptr;
}

const std::vector<UnitSpawn>& SpawnConfig::getUnits() const { return m_units; }

std::vector<UnitSpawn> SpawnConfig::getUnitsByType(const std::string& type) const {
    std::vector<UnitSpawn> result;
    for (const auto& u : m_units) {
        if (u.type == type) result.push_back(u);
    }
    return result;
}

int SpawnConfig::countType(const std::string& type) const {
    int c = 0;
    for (const auto& u : m_units) {
        if (u.type == type) c++;
    }
    return c;
}

int SpawnConfig::totalUnits() const { return static_cast<int>(m_units.size()); }

void SpawnConfig::clear() { m_units.clear(); }

// ════════════════════════════════════════════════════════════════════════════════
//  MAP DATA
// ════════════════════════════════════════════════════════════════════════════════

void SpawnConfig::setMapData(const MapInfo& info, const std::vector<std::vector<int>>& grid) {
    m_mapInfo = info;
    m_grid = grid;
    m_hasMapData = true;
}

const MapInfo& SpawnConfig::getMapInfo() const { return m_mapInfo; }
const std::vector<std::vector<int>>& SpawnConfig::getGrid() const { return m_grid; }
bool SpawnConfig::hasMapData() const { return m_hasMapData; }

// ════════════════════════════════════════════════════════════════════════════════
//  DETECTOR RADIUS
// ════════════════════════════════════════════════════════════════════════════════

void SpawnConfig::setDetectorRadius(double radius) {
    m_detectorRadius = (radius > 0.0) ? radius : 1.0;
}

double SpawnConfig::getDetectorRadius() const { return m_detectorRadius; }

// ════════════════════════════════════════════════════════════════════════════════
//  JSON SAVE
// ════════════════════════════════════════════════════════════════════════════════

void SpawnConfig::saveJSON(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for writing: " + filepath);
    }

    file << "{\n";

    // ── Map info section ──
    if (m_hasMapData) {
        file << "  \"map\": {\n";
        file << "    \"shp_path\": \"" << m_mapInfo.shpPath << "\",\n";
        file << "    \"cells_n\": " << m_mapInfo.cellsN << ",\n";
        file << "    \"canvas_width\": " << m_mapInfo.canvasWidth << ",\n";
        file << "    \"canvas_height\": " << m_mapInfo.canvasHeight << ",\n";
        file << "    \"min_depth\": " << m_mapInfo.minDepth << ",\n";
        file << "    \"max_depth\": " << m_mapInfo.maxDepth << ",\n";
        file << "    \"water_count\": " << m_mapInfo.waterCount << ",\n";
        file << "    \"land_count\": " << m_mapInfo.landCount << "\n";
        file << "  },\n";

        // ── Grid section (compact: one row per line) ──
        file << "  \"grid\": [\n";
        for (int r = 0; r < (int)m_grid.size(); r++) {
            file << "    [";
            for (int c = 0; c < (int)m_grid[r].size(); c++) {
                file << m_grid[r][c];
                if (c < (int)m_grid[r].size() - 1) file << ",";
            }
            file << "]";
            if (r < (int)m_grid.size() - 1) file << ",";
            file << "\n";
        }
        file << "  ],\n";
    }

    // ── Detector settings ──
    file << "  \"detector_radius\": " << m_detectorRadius << ",\n";

    // ── Units section ──
    file << "  \"units\": [\n";
    for (int i = 0; i < (int)m_units.size(); i++) {
        const auto& u = m_units[i];
        file << "    { \"type\": \"" << u.type
             << "\", \"row\": " << u.row
             << ", \"col\": " << u.col << " }";
        if (i < (int)m_units.size() - 1) file << ",";
        file << "\n";
    }
    file << "  ]\n";

    file << "}\n";
    file.close();

    std::cout << "Scenario saved to " << filepath << " (";
    if (m_hasMapData) {
        std::cout << m_mapInfo.cellsN << "x" << m_mapInfo.cellsN << " grid + ";
    }
    std::cout << m_units.size() << " units)\n";
}

// ════════════════════════════════════════════════════════════════════════════════
//  JSON LOAD
// ════════════════════════════════════════════════════════════════════════════════

// Helper: extract a string value after "key":
static std::string extractString(const std::string& content, const std::string& key, size_t searchFrom = 0) {
    size_t pos = content.find("\"" + key + "\"", searchFrom);
    if (pos == std::string::npos) return "";
    size_t colonPos = content.find(":", pos);
    size_t valStart = content.find("\"", colonPos + 1) + 1;
    size_t valEnd = content.find("\"", valStart);
    return content.substr(valStart, valEnd - valStart);
}

// Helper: extract a numeric value after "key":
static double extractNumber(const std::string& content, const std::string& key, size_t searchFrom = 0) {
    size_t pos = content.find("\"" + key + "\"", searchFrom);
    if (pos == std::string::npos) return 0.0;
    size_t colonPos = content.find(":", pos);
    // Skip whitespace after colon
    size_t valStart = colonPos + 1;
    while (valStart < content.size() && (content[valStart] == ' ' || content[valStart] == '\t'))
        valStart++;
    return std::stod(content.substr(valStart));
}

SpawnConfig SpawnConfig::loadJSON(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open spawn config: " + filepath);
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    SpawnConfig config;

    // ── Load map info if present ──
    if (content.find("\"map\"") != std::string::npos) {
        size_t mapSection = content.find("\"map\"");

        MapInfo info;
        info.shpPath      = extractString(content, "shp_path", mapSection);
        info.cellsN       = static_cast<int>(extractNumber(content, "cells_n", mapSection));
        info.canvasWidth   = static_cast<int>(extractNumber(content, "canvas_width", mapSection));
        info.canvasHeight  = static_cast<int>(extractNumber(content, "canvas_height", mapSection));
        info.minDepth     = extractNumber(content, "min_depth", mapSection);
        info.maxDepth     = extractNumber(content, "max_depth", mapSection);
        info.waterCount   = static_cast<int>(extractNumber(content, "water_count", mapSection));
        info.landCount    = static_cast<int>(extractNumber(content, "land_count", mapSection));

        // ── Load grid if present ──
        std::vector<std::vector<int>> grid;
        size_t gridPos = content.find("\"grid\"");
        if (gridPos != std::string::npos && info.cellsN > 0) {
            // Find the opening [ of the grid array
            size_t gridStart = content.find("[", gridPos);

            // Parse each row: find inner [...] arrays
            size_t pos = gridStart + 1;
            for (int r = 0; r < info.cellsN; r++) {
                size_t rowStart = content.find("[", pos);
                size_t rowEnd = content.find("]", rowStart);
                std::string rowStr = content.substr(rowStart + 1, rowEnd - rowStart - 1);

                std::vector<int> row;
                std::istringstream rowStream(rowStr);
                std::string cell;
                while (std::getline(rowStream, cell, ',')) {
                    // Trim whitespace
                    size_t first = cell.find_first_not_of(" \t\n\r");
                    if (first != std::string::npos) {
                        row.push_back(std::stoi(cell.substr(first)));
                    }
                }
                grid.push_back(row);
                pos = rowEnd + 1;
            }
        }

        config.setMapData(info, grid);
        std::cout << "Loaded map: " << info.cellsN << "x" << info.cellsN
                  << " (" << info.waterCount << " water, "
                  << info.landCount << " land)\n";
    }

    // ── Load detector radius if present ──
    if (content.find("\"detector_radius\"") != std::string::npos) {
        config.m_detectorRadius = extractNumber(content, "detector_radius");
    }

    // ── Load units ──
    size_t unitsPos = content.find("\"units\"");
    if (unitsPos != std::string::npos) {
        size_t pos = unitsPos;
        while (true) {
            pos = content.find("\"type\"", pos);
            if (pos == std::string::npos) break;

            size_t typeStart = content.find("\"", content.find(":", pos) + 1) + 1;
            size_t typeEnd = content.find("\"", typeStart);
            std::string type = content.substr(typeStart, typeEnd - typeStart);

            size_t rowPos = content.find("\"row\"", typeEnd);
            int row = static_cast<int>(extractNumber(content, "row", typeEnd));

            size_t colPos = content.find("\"col\"", rowPos);
            int col = static_cast<int>(extractNumber(content, "col", rowPos));

            config.addUnit(type, row, col);
            pos = colPos + 1;
        }
    }

    std::cout << "Loaded " << config.totalUnits() << " units from " << filepath << "\n";
    return config;
}

// ════════════════════════════════════════════════════════════════════════════════
//  DEBUG
// ════════════════════════════════════════════════════════════════════════════════

void SpawnConfig::printSummary() const {
    std::cout << "\n=== Scenario Configuration ===" << std::endl;

    if (m_hasMapData) {
        std::cout << "  Map source:   " << m_mapInfo.shpPath << std::endl;
        std::cout << "  Grid size:    " << m_mapInfo.cellsN << " x " << m_mapInfo.cellsN << std::endl;
        std::cout << "  Depth range:  [" << m_mapInfo.minDepth << ", " << m_mapInfo.maxDepth << "]" << std::endl;
        std::cout << "  Water cells:  " << m_mapInfo.waterCount << std::endl;
        std::cout << "  Land cells:   " << m_mapInfo.landCount << std::endl;
    }

    std::cout << "  Total units:  " << totalUnits() << std::endl;
    std::cout << "  Seekers:      " << countType("seeker") << std::endl;
    std::cout << "  Targets:      " << countType("target") << std::endl;
    std::cout << "  Detectors:    " << countType("detector") << std::endl;
    if (countType("detector") > 0) {
        std::cout << "  Det. radius:  " << m_detectorRadius << " cells" << std::endl;
    }

    for (const auto& u : m_units) {
        std::cout << "    " << u.type << " at (" << u.row << ", " << u.col << ")\n";
    }
    std::cout << "==============================\n" << std::endl;
}