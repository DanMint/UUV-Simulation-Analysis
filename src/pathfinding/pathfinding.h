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
     * Pre-allocates reusable flat buffers for A* to eliminate heap allocations.
     */
    Pathfinding(const std::vector<std::vector<int>>& grid);

    ~Pathfinding() = default;

    Pathfinding(const Pathfinding&) = delete;
    Pathfinding& operator=(const Pathfinding&) = delete;

    std::vector<Pos> findPath(int startRow, int startCol,
                              int destRow, int destCol) const;

    bool isValid(int row, int col) const;
    bool isPassable(int row, int col) const;
    int getLastNodesExpanded() const;
    double getLastPathCost() const;

private:
    const std::vector<std::vector<int>>& m_grid;
    int m_rows;
    int m_cols;
    mutable int m_lastNodesExpanded;
    mutable double m_lastPathCost;

    // Pre-allocated flat buffers for A* (eliminates per-call allocations)
    mutable std::vector<double> m_gFlat;
    mutable std::vector<double> m_fFlat;
    mutable std::vector<char> m_closedFlat;      // char avoids vector<bool> specialization
    mutable std::vector<Pos> m_parentFlat;
    mutable std::vector<std::pair<double, int>> m_openFlat;  // (f, idx) for binary heap

    static double octileHeuristic(int row, int col, int destRow, int destCol);

    // Flat-index helpers
    static inline int idx(int row, int col, int cols) noexcept { return row * cols + col; }
    static inline int rowFromIdx(int idx, int cols) noexcept { return idx / cols; }
    static inline int colFromIdx(int idx, int cols) noexcept { return idx % cols; }

    // Binary heap operations on flat array
    void heapPush(double f, int nodeIdx) const;
    std::pair<double, int> heapPop() const;
    bool heapEmpty() const;

    std::vector<Pos> tracePath(
        int destRow, int destCol) const;
};

#endif // PATHFINDING_H