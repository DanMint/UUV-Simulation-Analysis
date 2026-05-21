#ifndef SPAWNCONFIG_H
#define SPAWNCONFIG_H

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

/**
 * A single unit placement on the grid.
 * type ∈ { "seeker", "target", "detector", "interceptor" }
 */
struct UnitSpawn {
    std::string type;
    int row;
    int col;
};

/**
 * Map metadata stored alongside the config.
 */
struct MapInfo {
    std::string shpPath;
    int cellsN;
    int canvasWidth;
    int canvasHeight;
    double minDepth;
    double maxDepth;
    int waterCount;
    int landCount;
};

/**
 * SpawnConfig
 *
 * A complete scenario file containing:
 *   1. Map metadata (source file, dimensions, depth range)
 *   2. The full grid (2D water/land matrix)
 *   3. Unit placements (seekers, targets, detectors, interceptors)
 *   4. Detector sensing radius (single value applied to all detectors)
 *   5. Interceptor kill radius (single value applied to all interceptors)
 *   6. Max environmental noise level
 *
 * One JSON file = one complete, self-contained scenario.
 */
class SpawnConfig {
public:
    SpawnConfig() = default;

    // ─── Unit management ────────────────────────────────────────────

    bool addUnit(const std::string& type, int row, int col);
    bool removeUnit(int row, int col);
    const UnitSpawn* getUnitAt(int row, int col) const;
    const std::vector<UnitSpawn>& getUnits() const;
    std::vector<UnitSpawn> getUnitsByType(const std::string& type) const;
    int countType(const std::string& type) const;
    int totalUnits() const;
    void clear();

    // ─── Detector sensing radius ────────────────────────────────────

    /** Set the detection radius (cells) for all detectors. */
    void setDetectorRadius(double radius);
    /** Get the current detector sensing radius. Default 3.0. */
    double getDetectorRadius() const;

    // ─── Interceptor kill radius ────────────────────────────────────

    /** Set the kill radius (cells) for all interceptors. */
    void setInterceptorRadius(double radius);
    /** Get the current interceptor kill radius. Default 3.0. */
    double getInterceptorRadius() const;

    // ─── Noise level ─────────────────────────────────────────────────

    void setMaxNoiseLevel(double noise);
    double getMaxNoiseLevel() const;

    // ─── Map data ───────────────────────────────────────────────────

    void setMapData(const MapInfo& info, const std::vector<std::vector<int>>& grid);
    const MapInfo& getMapInfo() const;
    const std::vector<std::vector<int>>& getGrid() const;
    bool hasMapData() const;

    // ─── JSON I/O ───────────────────────────────────────────────────

    void saveJSON(const std::string& filepath) const;
    static SpawnConfig loadJSON(const std::string& filepath);

    // ─── Debug ──────────────────────────────────────────────────────

    void printSummary() const;

private:
    std::vector<UnitSpawn> m_units;
    MapInfo m_mapInfo = {};
    std::vector<std::vector<int>> m_grid;
    bool m_hasMapData = false;

    double m_detectorRadius    = 3.0;  // sensing radius (cells)
    double m_interceptorRadius = 3.0;  // kill radius (cells)
    double m_maxNoiseLevel     = 0.0;  // wave/wind noise level
};

#endif // SPAWNCONFIG_H