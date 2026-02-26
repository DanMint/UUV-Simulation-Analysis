#ifndef PATHFINDING_H
#define PATHFINDING_H

#include <vector>
#include <utility>
#include <string>

/**
 * Pathfinding
 *
 * A* search on a 2D grid with 8-directional movement.
 * Uses Octile distance as the heuristic function.
 *
 * Grid values:
 *   0 = water  (passable)
 *   1 = land   (blocked)
 *   2 = seeker (passable)
 *   3 = target (passable — it's the destination)
 *
 * Movement costs:
 *   Cardinal (up/down/left/right): 1.0
 *   Diagonal:                      sqrt(2) ≈ 1.414
 */
class Pathfinding {
public:
    using Pos = std::pair<int, int>;  // (row, col)

    /**
     * Construct with a reference to the grid.
     * Grid must outlive this object.
     */
    Pathfinding(const std::vector<std::vector<int>>& grid);

    /**
     * Find optimal path from start to destination using A* with Octile heuristic.
     *
     * @return Vector of (row, col) from start to destination (inclusive).
     *         Empty if no path exists.
     */
    std::vector<Pos> findPath(int startRow, int startCol,
                              int destRow, int destCol) const;

    /** Check if cell is within grid bounds. */
    bool isValid(int row, int col) const;

    /** Check if cell is passable (not land). */
    bool isPassable(int row, int col) const;

    /** Get the number of nodes expanded in the last search (for analysis). */
    int getLastNodesExpanded() const;

    /** Get the total path cost of the last search (for analysis). */
    double getLastPathCost() const;

private:
    const std::vector<std::vector<int>>& m_grid;
    int m_rows;
    int m_cols;

    // Stats from the last search (mutable so findPath can stay const)
    mutable int m_lastNodesExpanded;
    mutable double m_lastPathCost;

    /**
     * Octile distance heuristic.
     *
     * For 8-directional movement with diagonal cost √2:
     *   dx = |col - destCol|
     *   dy = |row - destRow|
     *   h  = max(dx, dy) + (√2 - 1) × min(dx, dy)
     *
     * This is the tightest admissible heuristic for 8-way grids.
     * Always ≤ true cost, so A* is guaranteed optimal.
     */
    static double octileHeuristic(int row, int col, int destRow, int destCol);

    /** Trace path from destination back to start using parent map. */
    static std::vector<Pos> tracePath(
        const std::vector<std::vector<Pos>>& parents,
        int destRow, int destCol);

    // 8 directions: cardinal + diagonal
    static constexpr int DIR_COUNT = 8;
    static constexpr int DR[DIR_COUNT] = { -1, 1, 0, 0, -1, -1, 1, 1 };
    static constexpr int DC[DIR_COUNT] = { 0, 0, -1, 1, -1, 1, -1, 1 };
    // Cost: 1.0 for cardinal (first 4), sqrt(2) for diagonal (last 4)
    static constexpr double MOVE_COST[DIR_COUNT] = {
        1.0, 1.0, 1.0, 1.0, 1.41421356, 1.41421356, 1.41421356, 1.41421356
    };
};

#endif // PATHFINDING_H