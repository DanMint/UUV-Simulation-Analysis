#ifndef MAPCREATION_H
#define MAPCREATION_H

#include <iostream>
#include <string>
#include <utility>
#include <vector>

class OGRPolygon;
struct UnitSpawn;

/**
 * MapCreation
 *
 * Reads a nautical .shp file containing depth polygons, scales the geometry to
 * fit a virtual canvas, and builds a 2D grid classifying each cell as terrain
 * or a broad unit category.
 *
 * Important category/type rule:
 *   - The grid stores only the broad category: seeker, target, detector, or
 *     interceptor.
 *   - The concrete unit type, such as "basic", remains in UnitSpawn and is not
 *     encoded as another grid-cell integer.
 *
 * Grid cell values:
 *   0 = water
 *   1 = land
 *   2 = seeker      (any seeker type)
 *   3 = target      (any target type)
 *   4 = detector    (any detector type)
 *   5 = interceptor (any interceptor type)
 */
class MapCreation {
public:
    // ─── Cell ID constants ──────────────────────────────────────────
    static constexpr int WATER       = 0;
    static constexpr int LAND        = 1;
    static constexpr int SEEKER      = 2;
    static constexpr int TARGET      = 3;
    static constexpr int DETECTOR    = 4;
    static constexpr int INTERCEPTOR = 5;

    /**
     * Read a .shp file and translate it into a square grid.
     *
     * @param shpPath Path to the .shp file.
     * @param cellsN Number of rows and columns in the square grid.
     * @param canvasWidth Width of the virtual drawing canvas.
     * @param canvasHeight Height of the virtual drawing canvas.
     */
    MapCreation(const std::string& shpPath,
                int cellsN = 100,
                int canvasWidth = 700,
                int canvasHeight = 700);

    /** Load a map from a cache file. */
    static MapCreation fromCache(const std::string& cachePath);

    ~MapCreation() = default;

    // ══════════════════════════════════════════════════════════════════════════
    //  SHAPEFILE LOADING
    // ══════════════════════════════════════════════════════════════════════════

    void extractPolygon(OGRPolygon* ogrPoly,
                        double depth1,
                        double depth2,
                        double minX,
                        double maxY,
                        double scale);

    void loadShapefile(const std::string& shpPath);

    // ══════════════════════════════════════════════════════════════════════════
    //  POINT-IN-POLYGON (Ray Casting Algorithm)
    // ══════════════════════════════════════════════════════════════════════════

    /**
     * Cast a horizontal ray from the point and count polygon-edge crossings.
     * An odd number of crossings means that the point is inside the polygon.
     */
    static bool pointInPolygon(
        double px,
        double py,
        const std::vector<std::pair<double, double>>& vertices);

    bool isPointInWater(double px, double py) const;

    // ══════════════════════════════════════════════════════════════════════════
    //  GRID CLASSIFICATION
    // ══════════════════════════════════════════════════════════════════════════

    void classifyCells();

    // ══════════════════════════════════════════════════════════════════════════
    //  SEAM GAP CLEANUP
    // ══════════════════════════════════════════════════════════════════════════

    /**
     * Repair thin land lines caused by small gaps between adjacent water
     * polygons in the source shapefile.
     */
    void cleanupSeamGaps();

    // ══════════════════════════════════════════════════════════════════════════
    //  GRID ACCESS
    // ══════════════════════════════════════════════════════════════════════════

    const std::vector<std::vector<int>>& getGrid() const;
    int getCell(int row, int col) const;
    bool isValid(int row, int col) const;
    bool isWater(int row, int col) const;
    bool isPassable(int row, int col) const;

    std::vector<std::pair<int, int>> getAllWaterCells() const;

    // ══════════════════════════════════════════════════════════════════════════
    //  UNIT PLACEMENT ON GRID
    // ══════════════════════════════════════════════════════════════════════════

    /**
     * Convert a broad category name into its grid-cell integer.
     *
     * @return SEEKER, TARGET, DETECTOR, or INTERCEPTOR.
     * @throws std::invalid_argument for an unknown category.
     */
    static int categoryToCellType(const std::string& category);

    /**
     * Place a unit category onto a water cell.
     *
     * @param unitType SEEKER, TARGET, DETECTOR, or INTERCEPTOR.
     * @return true when placement succeeds.
     */
    bool placeUnit(int row, int col, int unitType);

    /** Remove a unit and restore its cell to water. */
    bool removeUnit(int row, int col);

    /** Clear every unit category from the grid. */
    void clearAllUnits();

    /**
     * Place UnitSpawn objects from SpawnConfig.
     * Only unit.category affects the grid. unit.type remains available to the
     * simulation for concrete behavior selection.
     */
    int placeUnitsFromConfig(const std::vector<UnitSpawn>& units);

    /**
     * Compatibility overload for older callers whose string represented the
     * broad category directly.
     */
    int placeUnitsFromConfig(
        const std::vector<std::pair<std::string, std::pair<int, int>>>& units);

    // ══════════════════════════════════════════════════════════════════════════
    //  GRID INFO
    // ══════════════════════════════════════════════════════════════════════════

    int getCellsN() const;
    int getCanvasWidth() const;
    int getCanvasHeight() const;
    int getCellSize() const;
    int getWaterCount() const;
    int getLandCount() const;
    double getMinDepth() const;
    double getMaxDepth() const;

    // ══════════════════════════════════════════════════════════════════════════
    //  CACHE I/O
    // ══════════════════════════════════════════════════════════════════════════

    void saveCache(const std::string& filepath) const;
    void loadCache(const std::string& cachePath);

    // ══════════════════════════════════════════════════════════════════════════
    //  DEBUG / DISPLAY
    // ══════════════════════════════════════════════════════════════════════════

    void printGrid() const;
    void printStats() const;

    // ══════════════════════════════════════════════════════════════════════════
    //  HELPERS
    // ══════════════════════════════════════════════════════════════════════════

    static bool isUnitCell(int value);

private:
    /** Private blank constructor used only by fromCache(). */
    MapCreation() = default;

    int m_cellsInARow = 0;
    int m_canvasWidth = 0;
    int m_canvasHeight = 0;
    double m_colSpace = 0.0;
    double m_rowSpace = 0.0;
    int m_cellSize = 0;

    std::vector<std::vector<int>> m_grid;

    double m_minDepth = 0.0;
    double m_maxDepth = 0.0;

    struct ScaledPolygon {
        std::vector<std::pair<double, double>> vertices;
        double depth1 = 0.0;
        double depth2 = 0.0;
    };

    std::vector<ScaledPolygon> m_polygons;
};

#endif // MAPCREATION_H