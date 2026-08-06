#ifndef PATHFINDING_H
#define PATHFINDING_H

#include <string>
#include <utility>
#include <vector>

/**
 * Pathfinding
 *
 * A* search on a 2D grid with 8-directional movement.
 * Uses Octile distance as the heuristic function.
 *
 * Grid values supplied by MapCreation:
 *   0 = water       (passable)
 *   1 = land        (blocked)
 *   2 = seeker      (passable)
 *   3 = target      (passable)
 *   4 = detector    (passable)
 *   5 = interceptor (passable)
 *
 * Pathfinding intentionally uses only the grid category ID. Concrete unit
 * types such as "basic" live in SpawnConfig and do not affect passability.
 *
 * Movement costs:
 *   Cardinal: 1.0
 *   Diagonal: sqrt(2)
 */
class Pathfinding {
public:
    using Pos = std::pair<int, int>;  // (row, col)

    /** Construct with a grid reference. The grid must outlive this object. */
    explicit Pathfinding(const std::vector<std::vector<int>>& grid);

    /**
     * Find an optimal path from start to destination using A*.
     *
     * @return Positions from start to destination, inclusive. Empty when no
     *         path exists.
     */
    std::vector<Pos> findPath(
        int startRow,
        int startCol,
        int destRow,
        int destCol
    ) const;

    /** Check whether a cell is within grid bounds. */
    bool isValid(int row, int col) const;

    /** Check whether a cell is not land. */
    bool isPassable(int row, int col) const;

    /** Number of nodes expanded by the most recent search. */
    int getLastNodesExpanded() const;

    /** Path cost produced by the most recent successful search. */
    double getLastPathCost() const;

    /** Octile-distance heuristic for 8-directional movement. */
    static double octileHeuristic(
        int row,
        int col,
        int destRow,
        int destCol
    );

    /** Reconstruct a path from the destination through the parent map. */
    static std::vector<Pos> tracePath(
        const std::vector<std::vector<Pos>>& parents,
        int destRow,
        int destCol
    );

private:
    const std::vector<std::vector<int>>& m_grid;
    int m_rows;
    int m_cols;

    mutable int m_lastNodesExpanded;
    mutable double m_lastPathCost;

    static constexpr int DIR_COUNT = 8;
    static constexpr int DR[DIR_COUNT] = {
        -1, 1, 0, 0, -1, -1, 1, 1
    };
    static constexpr int DC[DIR_COUNT] = {
        0, 0, -1, 1, -1, 1, -1, 1
    };

    static constexpr double MOVE_COST[DIR_COUNT] = {
        1.0,
        1.0,
        1.0,
        1.0,
        1.41421356,
        1.41421356,
        1.41421356,
        1.41421356
    };
};

#endif // PATHFINDING_H