#ifndef SPAWNCONFIG_H
#define SPAWNCONFIG_H

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>

// ─── SpawnZone ───────────────────────────────────────────────────────────────

// A rectangular constraint region on the grid, shared by both the attacker
// and defender sides. Row/col values are inclusive grid indices.
//
// The attacker GA searches for optimal seeker positions within attacker zones.
// The defender GA searches for optimal defender positions within defender zones.
// Manual unit placement is unaffected by zones.
struct SpawnZone {
    int rowMin;
    int colMin;
    int rowMax;
    int colMax;

    // ── Unit counts requested for this zone ───────────────────────
    // Attacker zones use numSeekers; defender zones use the other two.
    // Default 0 means "no units of that category from this zone".
    int numSeekers      = 0;
    int numDetectors    = 0;
    int numInterceptors = 0;

    /** True if (row, col) falls inside this zone (inclusive). */
    bool contains(int row, int col) const;

    int width() const;
    int height() const;
    int area() const;
};

// ─── UnitSpawn ────────────────────────────────────────────────────────────────

/**
 * A single unit placement on the grid.
 *
 * category is the broad role:
 *   "seeker", "target", "detector", or "interceptor".
 *
 * type is the concrete implementation within that category:
 *   currently only "basic".
 *
 * Example:
 *   category = "interceptor"
 *   type     = "basic"
 */
struct UnitSpawn {
    std::string category;
    std::string type;
    int row;
    int col;
};

// ─── MapInfo ──────────────────────────────────────────────────────────────────

struct MapInfo {
    std::string shpPath;
    int cellsInARow;
    int canvasWidth;
    int canvasHeight;
    double minDepth;
    double maxDepth;
    int waterCount;
    int landCount;
};

// ─── SpawnConfig ─────────────────────────────────────────────────────────────
/**
 * Complete scenario descriptor:
 *   - Map metadata + full grid
 *   - Unit placements, each with a category and concrete type
 *   - Detector sensing radius and interceptor kill radius
 *   - Environmental noise level
 *   - Attacker spawn zones
 *   - Defender spawn zones
 *
 * Zones are optional. Without them the GA falls back to any water cell.
 * One JSON file = one complete, self-contained scenario.
 */
class SpawnConfig {
public:
    SpawnConfig() = default;

    // ─── Unit management ────────────────────────────────────────────

    /** Add a concrete unit. Returns false if the cell is occupied or either name is empty. */
    bool addUnit(const std::string& category,
                 const std::string& type,
                 int row,
                 int col);

    /**
     * Compatibility overload for older callers.
     * Treats the supplied value as a category and assigns type="basic".
     */
    bool addUnit(const std::string& category, int row, int col);

    bool removeUnit(int row, int col);
    const UnitSpawn* getUnitAt(int row, int col) const;
    const std::vector<UnitSpawn>& getUnits() const;

    std::vector<UnitSpawn> getUnitsByCategory(const std::string& category) const;
    std::vector<UnitSpawn> getUnitsByCategoryAndType(const std::string& category,
                                                     const std::string& type) const;

    int countCategory(const std::string& category) const;
    int countCategoryAndType(const std::string& category,
                             const std::string& type) const;

    /** Compatibility aliases for older code that used "type" to mean category. */
    std::vector<UnitSpawn> getUnitsByType(const std::string& category) const;
    int countType(const std::string& category) const;

    int totalUnits() const;
    void clear();

    // ─── Radii ──────────────────────────────────────────────────────

    void   setDetectorRadius(double radius);
    double getDetectorRadius() const;

    void   setInterceptorRadius(double radius);
    double getInterceptorRadius() const;

    // ─── Noise ──────────────────────────────────────────────────────

    void   setMaxNoiseLevel(double noise);
    double getMaxNoiseLevel() const;

    // ─── Attacker spawn zones ────────────────────────────────────────

    /** Add a normalized attacker zone and its requested seeker count. */
    void addAttackerZone(int rowMin, int colMin, int rowMax, int colMax,
                         int numSeekers = 0);

    bool removeAttackerZoneContaining(int row, int col);
    void clearAttackerZones();

    const std::vector<SpawnZone>& getAttackerZones() const;
    bool hasAttackerZones() const;

    // ─── Defender spawn zones ────────────────────────────────────────

    void addDefenderZone(int rowMin, int colMin, int rowMax, int colMax,
                         int numDetectors = 0, int numInterceptors = 0);
    bool removeDefenderZoneContaining(int row, int col);
    void clearDefenderZones();

    const std::vector<SpawnZone>& getDefenderZones() const;
    bool hasDefenderZones() const;

    // ─── Map data ────────────────────────────────────────────────────

    void setMapData(const MapInfo& info, const std::vector<std::vector<int>>& grid);
    const MapInfo& getMapInfo() const;
    const std::vector<std::vector<int>>& getGrid() const;
    bool hasMapData() const;

    // ─── JSON I/O ────────────────────────────────────────────────────

    void saveJSON(const std::string& filepath) const;
    static SpawnConfig loadJSON(const std::string& filepath);

    // ─── Debug ───────────────────────────────────────────────────────

    void printSummary() const;

private:
    std::vector<UnitSpawn> m_units;
    MapInfo m_mapInfo = {};
    std::vector<std::vector<int>> m_grid;
    bool m_hasMapData = false;

    double m_detectorRadius    = 3.0;
    double m_interceptorRadius = 3.0;
    double m_maxNoiseLevel     = 0.0;

    std::vector<SpawnZone> m_attackerZones;
    std::vector<SpawnZone> m_defenderZones;
};

#endif // SPAWNCONFIG_H