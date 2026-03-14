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
 */
struct UnitSpawn {
    std::string type;  // "seeker", "target", or "detector"
    int row;
    int col;
};

/**
 * Map metadata stored alongside the config.
 */
struct MapInfo {
    std::string shpPath;     // original shapefile path
    int cellsN;              // grid resolution (e.g. 100)
    int canvasWidth;         // virtual canvas width
    int canvasHeight;        // virtual canvas height
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
 *   3. Unit placements (seekers, targets, detectors)
 *   4. Detector radius (single value for all detectors)
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

    // ─── Detector radius ────────────────────────────────────────────

    /** Set the detection radius (in grid cells) for all detectors. */
    void setDetectorRadius(double radius);

    /** Get the current detector radius. Default is 3.0. */
    double getDetectorRadius() const;

    // ─── Map data ───────────────────────────────────────────────────

    /** Attach map info and grid data to this config. */
    void setMapData(const MapInfo& info, const std::vector<std::vector<int>>& grid);

    /** Get the stored map info. */
    const MapInfo& getMapInfo() const;

    /** Get the stored grid. Empty if no map data was attached. */
    const std::vector<std::vector<int>>& getGrid() const;

    /** Check if this config has map data. */
    bool hasMapData() const;

    // ─── JSON I/O ───────────────────────────────────────────────────

    /**
     * Save everything (map info + grid + units + detector radius) to a single JSON file.
     */
    void saveJSON(const std::string& filepath) const;

    /**
     * Load a complete scenario from JSON.
     * Contains map info, grid, unit placements, and detector radius.
     */
    static SpawnConfig loadJSON(const std::string& filepath);

    // ─── Debug ──────────────────────────────────────────────────────

    void printSummary() const;

private:
    std::vector<UnitSpawn> m_units;
    MapInfo m_mapInfo = {};
    std::vector<std::vector<int>> m_grid;
    bool m_hasMapData = false;
    double m_detectorRadius = 3.0;  // default radius in grid cells
};

#endif // SPAWNCONFIG_H