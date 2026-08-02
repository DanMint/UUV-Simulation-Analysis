#ifndef MAPCREATION_H
#define MAPCREATION_H

#include <string>
#include <vector>
#include <utility>
#include <iostream>

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
 *   2 = seeker      (attacker)
 *   3 = target      (defender, stationary)
 *   4 = detector    (defender sensor — sense only)
 *   5 = interceptor (defender effector — kill only)
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
     * Default constructor that reads a .shp file and translates it to a grid (std::vector<std::vector<int>> m_grid)
     * 
     * @param shpPath - path to the .shp file
     * @param cellsN - number of cells inside the 
     * @param canvasWidth - width of canvas
     * @param canvasHeight - height of canvas
     * 
    */
    MapCreation(const std::string& shpPath, int cellsN = 100, int canvasWidth = 700, int canvasHeight = 700);

    /**
     * Loads a map from cache 
     * 
     * @param cachePath - path to map file
    */
    static MapCreation fromCache(const std::string& cachePath);

    ~MapCreation() = default;

    // ════════════════════════════════════════════════════════════════════════════════
    //  SHAPEFILE LOADING
    // ════════════════════════════════════════════════════════════════════════════════

    void extractPolygon(OGRPolygon* ogrPoly,
                        double depth1, double depth2,
                        double minX, double maxY, double scale);

    void loadShapefile(const std::string& shpPath);

    // ════════════════════════════════════════════════════════════════════════════════
    //  POINT-IN-POLYGON (Ray Casting Algorithm)
    // ════════════════════════════════════════════════════════════════════════════════
    
    /**
     * Ray tracing algorithm. Cast a horizontal ray from the point to the right.Count how many polygon edges it crosses. If odd → inside. If even → outside.
     * 
     * @param px - which vector
     * @param py - what part in the vector
     * @param vertices - 
     */
    static bool pointInPolygon(double px, double py, const std::vector<std::pair<double, double>>& vertices);

    bool isPointInWater(double px, double py) const;

    // ════════════════════════════════════════════════════════════════════════════════
    //  GRID CLASSIFICATION
    // ════════════════════════════════════════════════════════════════════════════════

    void classifyCells();

    // ════════════════════════════════════════════════════════════════════════════════
    //  SEAM GAP CLEANUP
    // ════════════════════════════════════════════════════════════════════════════════
    
    /**
     * Seam gap cleanup. After initial classification thin lines of land can appear where adjacent polygons in the shapefile don't perfectly overlap (seam gaps).
    */
    void cleanupSeamGaps();

    // ════════════════════════════════════════════════════════════════════════════════
    //  GRID ACCESS
    // ════════════════════════════════════════════════════════════════════════════════

    const std::vector<std::vector<int>>& getGrid() const;
    int getCell(int row, int col) const;
    bool isValid(int row, int col) const;
    bool isWater(int row, int col) const;
    bool isPassable(int row, int col) const;

    std::vector<std::pair<int, int>> getAllWaterCells() const;

    // ════════════════════════════════════════════════════════════════════════════════
    //  UNIT PLACEMENT ON GRID
    // ════════════════════════════════════════════════════════════════════════════════

    /**
     * Place a unit onto the grid. Only succeeds on water cells (0).
     * @param unitType  SEEKER, TARGET, DETECTOR, or INTERCEPTOR
     */
    bool placeUnit(int row, int col, int unitType);

    /** Remove a unit (resets cell back to water). */
    bool removeUnit(int row, int col);

    /** Clear ALL units from the grid (resets every 2/3/4/5 back to 0). */
    void clearAllUnits();

    int placeUnitsFromConfig( const std::vector<std::pair<std::string, std::pair<int,int>>>& units);

    // ════════════════════════════════════════════════════════════════════════════════
    //  GRID INFO
    // ════════════════════════════════════════════════════════════════════════════════

    int getCellsN() const;
    int getCanvasWidth() const;
    int getCanvasHeight() const;
    int getCellSize() const;
    int getWaterCount() const;
    int getLandCount() const;
    double getMinDepth() const;
    double getMaxDepth() const;

   // ════════════════════════════════════════════════════════════════════════════════
   //  CACHE I/O
   // ════════════════════════════════════════════════════════════════════════════════

    void saveCache(const std::string& filepath) const;

    void loadCache(const std::string& cachePath);

   // ════════════════════════════════════════════════════════════════════════════════
   //  DEBUG / DISPLAY
   // ════════════════════════════════════════════════════════════════════════════════

    void printGrid() const;
    void printStats() const;

   // ════════════════════════════════════════════════════════════════════════════════
   //  HELPER 
   // ════════════════════════════════════════════════════════════════════════════════

   static bool isUnitCell(int v);

private:
    int m_cellsInARow;
    int m_canvasWidth;
    int m_canvasHeight;
    double m_colSpace;
    double m_rowSpace;
    int m_cellSize;

    std::vector<std::vector<int>> m_grid;

    double m_minDepth;
    double m_maxDepth;

    struct ScaledPolygon {
        std::vector<std::pair<double, double>> vertices;
        double depth1;
        double depth2;
    };
    std::vector<ScaledPolygon> m_polygons;
};

#endif // MAPCREATION_H