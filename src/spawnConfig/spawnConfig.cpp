#include "spawnConfig.h"

// ════════════════════════════════════════════════════════════════════════════════
//  UNIT MANAGEMENT  (unchanged)
// ════════════════════════════════════════════════════════════════════════════════

bool SpawnConfig::addUnit(const std::string& type, int row, int col) {
    for (const auto& u : m_units)
        if (u.row == row && u.col == col) return false;
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
    for (const auto& u : m_units)
        if (u.row == row && u.col == col) return &u;
    return nullptr;
}

const std::vector<UnitSpawn>& SpawnConfig::getUnits() const { return m_units; }

std::vector<UnitSpawn> SpawnConfig::getUnitsByType(const std::string& type) const {
    std::vector<UnitSpawn> result;
    for (const auto& u : m_units)
        if (u.type == type) result.push_back(u);
    return result;
}

int SpawnConfig::countType(const std::string& type) const {
    int c = 0;
    for (const auto& u : m_units) if (u.type == type) c++;
    return c;
}

int  SpawnConfig::totalUnits() const { return static_cast<int>(m_units.size()); }
void SpawnConfig::clear()            { m_units.clear(); }

// ════════════════════════════════════════════════════════════════════════════════
//  MAP DATA  (unchanged)
// ════════════════════════════════════════════════════════════════════════════════

void SpawnConfig::setMapData(const MapInfo& info,
                             const std::vector<std::vector<int>>& grid) {
    m_mapInfo = info;
    m_grid    = grid;
    m_hasMapData = true;
}

const MapInfo& SpawnConfig::getMapInfo() const { return m_mapInfo; }
const std::vector<std::vector<int>>& SpawnConfig::getGrid() const { return m_grid; }
bool SpawnConfig::hasMapData() const { return m_hasMapData; }

// ════════════════════════════════════════════════════════════════════════════════
//  RADII  (unchanged)
// ════════════════════════════════════════════════════════════════════════════════

void   SpawnConfig::setDetectorRadius(double r)    { m_detectorRadius    = (r > 0) ? r : 1.0; }
double SpawnConfig::getDetectorRadius() const       { return m_detectorRadius; }

void   SpawnConfig::setInterceptorRadius(double r) { m_interceptorRadius = (r > 0) ? r : 1.0; }
double SpawnConfig::getInterceptorRadius() const    { return m_interceptorRadius; }

// ════════════════════════════════════════════════════════════════════════════════
//  NOISE  (unchanged)
// ════════════════════════════════════════════════════════════════════════════════

void   SpawnConfig::setMaxNoiseLevel(double n) { m_maxNoiseLevel = (n >= 0) ? n : 0.0; }
double SpawnConfig::getMaxNoiseLevel() const    { return m_maxNoiseLevel; }

// ════════════════════════════════════════════════════════════════════════════════
//  ATTACKER SPAWN ZONES
// ════════════════════════════════════════════════════════════════════════════════

void SpawnConfig::addAttackerZone(int rowMin, int colMin, int rowMax, int colMax,
                                  int numSeekers) {
    SpawnZone z;
    z.rowMin = std::min(rowMin, rowMax);
    z.colMin = std::min(colMin, colMax);
    z.rowMax = std::max(rowMin, rowMax);
    z.colMax = std::max(colMin, colMax);
    z.numSeekers = (numSeekers >= 0) ? numSeekers : 0;
    m_attackerZones.push_back(z);
}

bool SpawnConfig::removeAttackerZoneContaining(int row, int col) {
    for (auto it = m_attackerZones.begin(); it != m_attackerZones.end(); ++it) {
        if (it->contains(row, col)) {
            m_attackerZones.erase(it);
            return true;
        }
    }
    return false;
}

void SpawnConfig::clearAttackerZones()                           { m_attackerZones.clear(); }
const std::vector<SpawnZone>& SpawnConfig::getAttackerZones() const { return m_attackerZones; }
bool SpawnConfig::hasAttackerZones() const                       { return !m_attackerZones.empty(); }

// ════════════════════════════════════════════════════════════════════════════════
//  DEFENDER SPAWN ZONES
// ════════════════════════════════════════════════════════════════════════════════

void SpawnConfig::addDefenderZone(int rowMin, int colMin, int rowMax, int colMax,
                                  int numDetectors, int numInterceptors) {
    SpawnZone z;
    z.rowMin = std::min(rowMin, rowMax);
    z.colMin = std::min(colMin, colMax);
    z.rowMax = std::max(rowMin, rowMax);
    z.colMax = std::max(colMin, colMax);
    z.numDetectors    = (numDetectors    >= 0) ? numDetectors    : 0;
    z.numInterceptors = (numInterceptors >= 0) ? numInterceptors : 0;
    m_defenderZones.push_back(z);
}

bool SpawnConfig::removeDefenderZoneContaining(int row, int col) {
    for (auto it = m_defenderZones.begin(); it != m_defenderZones.end(); ++it) {
        if (it->contains(row, col)) {
            m_defenderZones.erase(it);
            return true;
        }
    }
    return false;
}

void SpawnConfig::clearDefenderZones()                           { m_defenderZones.clear(); }
const std::vector<SpawnZone>& SpawnConfig::getDefenderZones() const { return m_defenderZones; }
bool SpawnConfig::hasDefenderZones() const                       { return !m_defenderZones.empty(); }

// ════════════════════════════════════════════════════════════════════════════════
//  JSON HELPERS  (file-scope)
// ════════════════════════════════════════════════════════════════════════════════

static std::string extractString(const std::string& content,
                                 const std::string& key,
                                 size_t from = 0) {
    size_t pos = content.find("\"" + key + "\"", from);
    if (pos == std::string::npos) return "";
    size_t colon  = content.find(":", pos);
    size_t vStart = content.find("\"", colon + 1) + 1;
    size_t vEnd   = content.find("\"", vStart);
    return content.substr(vStart, vEnd - vStart);
}

static double extractNumber(const std::string& content,
                            const std::string& key,
                            size_t from = 0) {
    size_t pos = content.find("\"" + key + "\"", from);
    if (pos == std::string::npos) return 0.0;
    size_t colon = content.find(":", pos);
    size_t vs    = colon + 1;
    while (vs < content.size() && (content[vs] == ' ' || content[vs] == '\t')) vs++;
    return std::stod(content.substr(vs));
}

/** Parse an array of SpawnZone objects: [{"row_min":…,"col_min":…,…},…] */
static std::vector<SpawnZone> parseZoneArray(const std::string& content,
                                             const std::string& arrayKey) {
    std::vector<SpawnZone> zones;
    size_t keyPos = content.find("\"" + arrayKey + "\"");
    if (keyPos == std::string::npos) return zones;

    size_t arrOpen = content.find("[", keyPos);
    if (arrOpen == std::string::npos) return zones;

    // Walk forward to find the matching ']', counting brackets
    int depth = 0;
    size_t arrClose = std::string::npos;
    for (size_t i = arrOpen; i < content.size(); i++) {
        if      (content[i] == '[') depth++;
        else if (content[i] == ']') { if (--depth == 0) { arrClose = i; break; } }
    }
    if (arrClose == std::string::npos) return zones;

    // Parse each {…} object within [arrOpen+1 .. arrClose-1]
    size_t pos = arrOpen + 1;
    while (pos < arrClose) {
        size_t objOpen  = content.find("{", pos);
        if (objOpen  == std::string::npos || objOpen  >= arrClose) break;
        size_t objClose = content.find("}", objOpen);
        if (objClose == std::string::npos || objClose >= arrClose) break;

        // Extract the substring for this single zone object
        std::string obj = content.substr(objOpen, objClose - objOpen + 1);

        SpawnZone z;
        z.rowMin = static_cast<int>(extractNumber(obj, "row_min"));
        z.colMin = static_cast<int>(extractNumber(obj, "col_min"));
        z.rowMax = static_cast<int>(extractNumber(obj, "row_max"));
        z.colMax = static_cast<int>(extractNumber(obj, "col_max"));
        z.numSeekers      = static_cast<int>(extractNumber(obj, "num_seekers"));
        z.numDetectors    = static_cast<int>(extractNumber(obj, "num_detectors"));
        z.numInterceptors = static_cast<int>(extractNumber(obj, "num_interceptors"));
        zones.push_back(z);

        pos = objClose + 1;
    }
    return zones;
}

// ════════════════════════════════════════════════════════════════════════════════
//  JSON SAVE
// ════════════════════════════════════════════════════════════════════════════════

void SpawnConfig::saveJSON(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file for writing: " + filepath);

    file << "{\n";

    // ── Map info ──
    if (m_hasMapData) {
        file << "  \"map\": {\n";
        file << "    \"shp_path\": \""    << m_mapInfo.shpPath    << "\",\n";
        file << "    \"cells_n\": "       << m_mapInfo.cellsN     << ",\n";
        file << "    \"canvas_width\": "  << m_mapInfo.canvasWidth  << ",\n";
        file << "    \"canvas_height\": " << m_mapInfo.canvasHeight << ",\n";
        file << "    \"min_depth\": "     << m_mapInfo.minDepth   << ",\n";
        file << "    \"max_depth\": "     << m_mapInfo.maxDepth   << ",\n";
        file << "    \"water_count\": "   << m_mapInfo.waterCount << ",\n";
        file << "    \"land_count\": "    << m_mapInfo.landCount  << "\n";
        file << "  },\n";

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

    // ── Defender settings ──
    file << "  \"detector_radius\": "    << m_detectorRadius    << ",\n";
    file << "  \"interceptor_radius\": " << m_interceptorRadius << ",\n";
    file << "  \"max_noise_level\": "    << m_maxNoiseLevel     << ",\n";

    // ── Attacker spawn zones ──
    file << "  \"attacker_zones\": [\n";
    for (int i = 0; i < (int)m_attackerZones.size(); i++) {
        const auto& z = m_attackerZones[i];
        file << "    { \"row_min\": " << z.rowMin
             << ", \"col_min\": "     << z.colMin
             << ", \"row_max\": "     << z.rowMax
             << ", \"col_max\": "     << z.colMax
             << ", \"num_seekers\": " << z.numSeekers << " }";
        if (i < (int)m_attackerZones.size() - 1) file << ",";
        file << "\n";
    }
    file << "  ],\n";

    // ── Defender spawn zones ──
    file << "  \"defender_zones\": [\n";
    for (int i = 0; i < (int)m_defenderZones.size(); i++) {
        const auto& z = m_defenderZones[i];
        file << "    { \"row_min\": "          << z.rowMin
             << ", \"col_min\": "              << z.colMin
             << ", \"row_max\": "              << z.rowMax
             << ", \"col_max\": "              << z.colMax
             << ", \"num_detectors\": "        << z.numDetectors
             << ", \"num_interceptors\": "     << z.numInterceptors << " }";
        if (i < (int)m_defenderZones.size() - 1) file << ",";
        file << "\n";
    }
    file << "  ],\n";

    // ── Units ──
    file << "  \"units\": [\n";
    for (int i = 0; i < (int)m_units.size(); i++) {
        const auto& u = m_units[i];
        file << "    { \"type\": \"" << u.type
             << "\", \"row\": "      << u.row
             << ", \"col\": "        << u.col << " }";
        if (i < (int)m_units.size() - 1) file << ",";
        file << "\n";
    }
    file << "  ]\n";

    file << "}\n";
    file.close();

    std::cout << "Scenario saved to " << filepath << " (";
    if (m_hasMapData) std::cout << m_mapInfo.cellsN << "x" << m_mapInfo.cellsN << " grid + ";
    std::cout << m_units.size()       << " units, "
              << m_attackerZones.size() << " ATK zones, "
              << m_defenderZones.size() << " DEF zones)\n";
}

// ════════════════════════════════════════════════════════════════════════════════
//  JSON LOAD
// ════════════════════════════════════════════════════════════════════════════════

SpawnConfig SpawnConfig::loadJSON(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open())
        throw std::runtime_error("Cannot open spawn config: " + filepath);

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    SpawnConfig config;

    // ── Map info ──
    if (content.find("\"map\"") != std::string::npos) {
        size_t ms = content.find("\"map\"");
        MapInfo info;
        info.shpPath      = extractString(content, "shp_path",      ms);
        info.cellsN       = static_cast<int>(extractNumber(content, "cells_n",       ms));
        info.canvasWidth  = static_cast<int>(extractNumber(content, "canvas_width",  ms));
        info.canvasHeight = static_cast<int>(extractNumber(content, "canvas_height", ms));
        info.minDepth     = extractNumber(content, "min_depth",  ms);
        info.maxDepth     = extractNumber(content, "max_depth",  ms);
        info.waterCount   = static_cast<int>(extractNumber(content, "water_count", ms));
        info.landCount    = static_cast<int>(extractNumber(content, "land_count",  ms));

        std::vector<std::vector<int>> grid;
        size_t gridPos = content.find("\"grid\"");
        if (gridPos != std::string::npos && info.cellsN > 0) {
            size_t gStart = content.find("[", gridPos);
            size_t pos    = gStart + 1;
            for (int r = 0; r < info.cellsN; r++) {
                size_t rowS = content.find("[", pos);
                size_t rowE = content.find("]", rowS);
                std::string rowStr = content.substr(rowS + 1, rowE - rowS - 1);
                std::vector<int> row;
                std::istringstream ss(rowStr);
                std::string cell;
                while (std::getline(ss, cell, ',')) {
                    size_t first = cell.find_first_not_of(" \t\n\r");
                    if (first != std::string::npos)
                        row.push_back(std::stoi(cell.substr(first)));
                }
                grid.push_back(row);
                pos = rowE + 1;
            }
        }
        config.setMapData(info, grid);
        std::cout << "Loaded map: " << info.cellsN << "x" << info.cellsN
                  << " (" << info.waterCount << " water, " << info.landCount << " land)\n";
    }

    // ── Radii / noise ──
    if (content.find("\"detector_radius\"")    != std::string::npos)
        config.m_detectorRadius    = extractNumber(content, "detector_radius");
    if (content.find("\"interceptor_radius\"") != std::string::npos)
        config.m_interceptorRadius = extractNumber(content, "interceptor_radius");
    if (content.find("\"max_noise_level\"")    != std::string::npos)
        config.m_maxNoiseLevel     = extractNumber(content, "max_noise_level");

    // ── Attacker zones — new format ──
    if (content.find("\"attacker_zones\"") != std::string::npos) {
        config.m_attackerZones = parseZoneArray(content, "attacker_zones");
        if (!config.m_attackerZones.empty())
            std::cout << "Loaded " << config.m_attackerZones.size()
                      << " attacker zone(s)\n";
    }
    // ── Attacker zone — old single-zone format (backward compat) ──
    else if (content.find("\"attacker_spawn_region\"") != std::string::npos) {
        size_t zp = content.find("\"attacker_spawn_region\"");
        config.addAttackerZone(
            static_cast<int>(extractNumber(content, "row_min", zp)),
            static_cast<int>(extractNumber(content, "col_min", zp)),
            static_cast<int>(extractNumber(content, "row_max", zp)),
            static_cast<int>(extractNumber(content, "col_max", zp)));
        std::cout << "Loaded 1 attacker zone (legacy format)\n";
    }

    // ── Defender zones ──
    if (content.find("\"defender_zones\"") != std::string::npos) {
        config.m_defenderZones = parseZoneArray(content, "defender_zones");
        if (!config.m_defenderZones.empty())
            std::cout << "Loaded " << config.m_defenderZones.size()
                      << " defender zone(s)\n";
    }

    // ── Units ──
    size_t unitsPos = content.find("\"units\"");
    if (unitsPos != std::string::npos) {
        size_t pos = unitsPos;
        while (true) {
            pos = content.find("\"type\"", pos);
            if (pos == std::string::npos) break;
            size_t tStart = content.find("\"", content.find(":", pos) + 1) + 1;
            size_t tEnd   = content.find("\"", tStart);
            std::string type = content.substr(tStart, tEnd - tStart);

            size_t rowPos = content.find("\"row\"", tEnd);
            int row = static_cast<int>(extractNumber(content, "row", tEnd));
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
//  PRINT SUMMARY
// ════════════════════════════════════════════════════════════════════════════════

void SpawnConfig::printSummary() const {
    std::cout << "\n=== Scenario Configuration ===" << std::endl;

    if (m_hasMapData) {
        std::cout << "  Map source:   " << m_mapInfo.shpPath << std::endl;
        std::cout << "  Grid size:    " << m_mapInfo.cellsN << " x " << m_mapInfo.cellsN << std::endl;
        std::cout << "  Depth range:  [" << m_mapInfo.minDepth << ", " << m_mapInfo.maxDepth << "]" << std::endl;
        std::cout << "  Water cells:  " << m_mapInfo.waterCount << std::endl;
        std::cout << "  Land cells:   " << m_mapInfo.landCount  << std::endl;
    }

    std::cout << "  Seekers:      " << countType("seeker")      << std::endl;
    std::cout << "  Targets:      " << countType("target")      << std::endl;
    std::cout << "  Detectors:    " << countType("detector")    << std::endl;
    std::cout << "  Interceptors: " << countType("interceptor") << std::endl;
    if (countType("detector")    > 0) std::cout << "  Det. radius:  " << m_detectorRadius    << " cells\n";
    if (countType("interceptor") > 0) std::cout << "  Int. radius:  " << m_interceptorRadius << " cells\n";
    std::cout << "  Noise level:  " << m_maxNoiseLevel
              << (m_maxNoiseLevel > 0 ? " (enabled)" : " (off)") << std::endl;

    if (!m_attackerZones.empty()) {
        int totalSeekers = 0;
        for (const auto& z : m_attackerZones) totalSeekers += z.numSeekers;
        std::cout << "  Attacker zones: " << m_attackerZones.size()
                  << "  (GA will spawn " << totalSeekers << " seeker(s) total)\n";
        for (int i = 0; i < (int)m_attackerZones.size(); i++) {
            const auto& z = m_attackerZones[i];
            std::cout << "    Zone " << (i+1) << ": rows " << z.rowMin << "-" << z.rowMax
                      << ", cols " << z.colMin << "-" << z.colMax
                      << "  (" << z.width() << "x" << z.height() << ")"
                      << "  seekers=" << z.numSeekers << "\n";
        }
    }

    if (!m_defenderZones.empty()) {
        int totalDet = 0, totalInt = 0;
        for (const auto& z : m_defenderZones) {
            totalDet += z.numDetectors;
            totalInt += z.numInterceptors;
        }
        std::cout << "  Defender zones: " << m_defenderZones.size()
                  << "  (GA will spawn " << totalDet << " detector(s), "
                  << totalInt << " interceptor(s) total)\n";
        for (int i = 0; i < (int)m_defenderZones.size(); i++) {
            const auto& z = m_defenderZones[i];
            std::cout << "    Zone " << (i+1) << ": rows " << z.rowMin << "-" << z.rowMax
                      << ", cols " << z.colMin << "-" << z.colMax
                      << "  (" << z.width() << "x" << z.height() << ")"
                      << "  det=" << z.numDetectors
                      << "  int=" << z.numInterceptors << "\n";
        }
    }

    std::cout << "==============================\n" << std::endl;
}