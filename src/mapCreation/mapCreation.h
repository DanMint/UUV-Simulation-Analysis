#ifndef MAPCREATION_H
#define MAPCREATION_H

#include <string>
#include <vector>
#include <utility>
#include <iostream>

// Forward declaration so the header doesn't need GDAL includes
class OGRPolygon;

/**
 * MapCreation
 *
 * Reads a nautical .shp file containing depth polygons,
 * scales the geometry to fit a virtual canvas, and builds
 * a 2D grid classifying each cell as water or land.
 *
 * Grid cell values:
 *   0 = water
 *   1 = land
 *   2 = seeker (attacker)
 *   3 = target (defender)
 *   4 = detector (interceptor — defender side)
 */
class MapCreation {
public:
    // ─── Cell ID constants ──────────────────────────────────────────
    static constexpr int WATER    = 0;
    static constexpr int LAND     = 1;
    static constexpr int SEEKER   = 2;
    static constexpr int TARGET   = 3;
    static constexpr int DETECTOR = 4;

    // ─── Constructor / Factory ──────────────────────────────────────

    MapCreation(const std::string& shpPath, int cellsN = 100,
                int canvasWidth = 700, int canvasHeight = 700);

    static MapCreation fromCache(const std::string& cachePath);

    ~MapCreation() = default;

    // ─── Grid Access ────────────────────────────────────────────────

    const std::vector<std::vector<int>>& getGrid() const;
    int getCell(int row, int col) const;
    bool isValid(int row, int col) const;

    /** Check if cell is water (0) — empty, no unit. */
    bool isWater(int row, int col) const;

    /** Check if cell is passable (anything except land). */
    bool isPassable(int row, int col) const;

    /** Get all empty water cell coordinates as (col, row) pairs. */
    std::vector<std::pair<int, int>> getAllWaterCells() const;

    // ─── Unit Placement ─────────────────────────────────────────────

    /**
     * Place a unit onto the grid. Only succeeds on water cells (0).
     * @param unitType  Use SEEKER (2), TARGET (3), or DETECTOR (4)
     * @return true if placed, false if cell is land, occupied, or out of bounds.
     */
    bool placeUnit(int row, int col, int unitType);

    /**
     * Remove a unit from the grid (resets cell back to water).
     * @return true if a unit was actually removed.
     */
    bool removeUnit(int row, int col);

    /** Clear ALL units from the grid (reset every 2/3/4 back to 0). */
    void clearAllUnits();

    /**
     * Place units from a list of (type_string, (row, col)) pairs.
     * @return number of units successfully placed.
     */
    int placeUnitsFromConfig(
        const std::vector<std::pair<std::string, std::pair<int,int>>>& units);

    // ─── Grid Info ──────────────────────────────────────────────────

    int getCellsN() const;
    int getCanvasWidth() const;
    int getCanvasHeight() const;
    int getCellSize() const;
    int getWaterCount() const;
    int getLandCount() const;
    double getMinDepth() const;
    double getMaxDepth() const;

    // ─── Cache I/O ──────────────────────────────────────────────────

    void saveCache(const std::string& filepath) const;

    // ─── Debug / Display ────────────────────────────────────────────

    void printGrid() const;
    void printStats() const;

private:
    // ─── Grid dimensions ────────────────────────────────────────────
    int m_cellsN;
    int m_canvasWidth;
    int m_canvasHeight;
    double m_colSpace;
    double m_rowSpace;
    int m_cellSize;

    // ─── The grid itself ────────────────────────────────────────────
    std::vector<std::vector<int>> m_grid;

    // ─── Map data ───────────────────────────────────────────────────
    double m_minDepth;
    double m_maxDepth;

    struct ScaledPolygon {
        std::vector<std::pair<double, double>> vertices;
        double depth1;
        double depth2;
    };

    std::vector<ScaledPolygon> m_polygons;

    // ─── Private methods ────────────────────────────────────────────

    void loadShapefile(const std::string& shpPath);
    void classifyCells();
    void cleanupSeamGaps();
    void loadCache(const std::string& cachePath);

    static bool pointInPolygon(double px, double py,
                               const std::vector<std::pair<double, double>>& vertices);

    bool isPointInWater(double px, double py) const;

    void extractPolygon(OGRPolygon* ogrPoly,
                        double depth1, double depth2,
                        double minX, double maxY, double scale);
};

#endif // MAPCREATION_H