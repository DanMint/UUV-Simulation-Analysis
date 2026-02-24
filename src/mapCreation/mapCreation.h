#pragma once

#include <string>
#include <vector>
#include <utility>  // std::pair
#include <iostream>

// Forward declaration so the header doesn't need GDAL includes. This reduces compile-time and dependecy spreading
class OGRPolygon;

/**
 * MapCreation
 *
 * Reads a nautical .shp file containing depth polygons,
 * scales the geometry to fit a virtual canvas, and builds
 * a 2D grid classifying each cell as water (0) or land (1).
 *
 * Dependencies: GDAL/OGR (for reading .shp files)
 *
 * The shapefile is expected to have columns:
 *   - geometry : Polygon / MultiPolygon
 *   - DRVAL1   : shallow depth bound
 *   - DRVAL2   : deeper depth bound
 */
class MapCreation {
public:
    // ─── Constructor / Destructor ───────────────────────────────────

    /**
     * Build grid from a shapefile.
     * @param shpPath      Path to the .shp file
     * @param cellsN       Number of cells per row/column (default 100 = 100x100 grid)
     * @param canvasWidth   Virtual canvas width in pixels (default 700)
     * @param canvasHeight  Virtual canvas height in pixels (default 700)
     */
    MapCreation(const std::string& shpPath, int cellsN = 100,
                int canvasWidth = 700, int canvasHeight = 700);

    /**
     * Build grid from a previously saved cache file (no .shp needed).
     * Use the static factory method to make intent clear.
     */
    static MapCreation fromCache(const std::string& cachePath);

    ~MapCreation() = default;

    // ─── Grid Access ────────────────────────────────────────────────

    /** Get the full grid (0 = water, 1 = land). */
    const std::vector<std::vector<int>>& getGrid() const;

    /** Get cell value at (row, col). Returns -1 if out of bounds. */
    int getCell(int row, int col) const;

    /** Check if (row, col) is within grid bounds. */
    bool isValid(int row, int col) const;

    /** Check if cell at (row, col) is water. */
    bool isWater(int row, int col) const;

    /** Get all water cell coordinates as (col, row) pairs. */
    std::vector<std::pair<int, int>> getAllWaterCells() const;

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

    /** Save the grid to a simple text file for fast reloading. */
    void saveCache(const std::string& filepath) const;

    // ─── Debug / Display ────────────────────────────────────────────

    /** Print an ASCII representation of the grid (. = water, # = land). */
    void printGrid() const;

    /** Print summary statistics. */
    void printStats() const;

private:
    // ─── Grid dimensions ────────────────────────────────────────────
    int m_cellsN;
    int m_canvasWidth;
    int m_canvasHeight;
    double m_colSpace;   // pixel width of one cell
    double m_rowSpace;   // pixel height of one cell
    int m_cellSize;      // integer cell size (canvasWidth / cellsN)

    // ─── The grid itself ────────────────────────────────────────────
    std::vector<std::vector<int>> m_grid;  // [row][col], 0 = water, 1 = land

    // ─── Map data ───────────────────────────────────────────────────
    double m_minDepth;
    double m_maxDepth;

    /**
     * A single scaled polygon stored as a list of (x, y) vertex pairs
     * in pixel-space coordinates, plus its depth values.
     */
    struct ScaledPolygon {
        std::vector<std::pair<double, double>> vertices;
        double depth1;
        double depth2;
    };

    /** All water polygons from the shapefile, scaled to pixel space. */
    std::vector<ScaledPolygon> m_polygons;

    // ─── Private methods ────────────────────────────────────────────

    /** Read the .shp file and populate m_polygons with scaled geometry. */
    void loadShapefile(const std::string& shpPath);

    /** Walk the grid and classify each cell using 5-point sampling. */
    void classifyCells();

    /** Load grid from a cache file. */
    void loadCache(const std::string& cachePath);

    /**
     * Ray-casting point-in-polygon test.
     * Returns true if point (px, py) is inside the polygon.
     */
    static bool pointInPolygon(double px, double py,
                               const std::vector<std::pair<double, double>>& vertices);

    /**
     * Check if a pixel-space point is inside ANY water polygon.
     */
    bool isPointInWater(double px, double py) const;

    /**
     * Helper: extract exterior ring from an OGR polygon, scale it,
     * and push it onto m_polygons.
     */
    void extractPolygon(OGRPolygon* ogrPoly,
                        double depth1, double depth2,
                        double minX, double maxY, double scale);
};
