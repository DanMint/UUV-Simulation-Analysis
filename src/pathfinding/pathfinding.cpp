#include "pathfinding.h"

#include <queue>
#include <cmath>
#include <limits>
#include <algorithm>
#include <iostream>

// ════════════════════════════════════════════════════════════════════════════════
//  CONSTRUCTOR
// ════════════════════════════════════════════════════════════════════════════════

Pathfinding::Pathfinding(const std::vector<std::vector<int>>& grid)
    : m_grid(grid),
      m_lastNodesExpanded(0),
      m_lastPathCost(0.0)
{
    m_rows = static_cast<int>(grid.size());
    m_cols = (m_rows > 0) ? static_cast<int>(grid[0].size()) : 0;
}

// ════════════════════════════════════════════════════════════════════════════════
//  HELPERS
// ════════════════════════════════════════════════════════════════════════════════

bool Pathfinding::isValid(int row, int col) const {
    return row >= 0 && row < m_rows && col >= 0 && col < m_cols;
}

bool Pathfinding::isPassable(int row, int col) const {
    // Land (1) is the only blocked cell type
    // Water (0), seeker (2), target (3) are all passable
    return isValid(row, col) && m_grid[row][col] != 1;
}

int Pathfinding::getLastNodesExpanded() const { return m_lastNodesExpanded; }
double Pathfinding::getLastPathCost() const { return m_lastPathCost; }

// ════════════════════════════════════════════════════════════════════════════════
//  OCTILE HEURISTIC
// ════════════════════════════════════════════════════════════════════════════════
//
//  Why Octile?
//
//  On a grid with 8-directional movement:
//    - Cardinal moves (up/down/left/right) cost 1.0
//    - Diagonal moves cost √2 ≈ 1.414
//
//  To go from A to B, the optimal strategy is:
//    1. Move diagonally for min(dx, dy) steps  → each costs √2
//    2. Move straight for |dx - dy| steps      → each costs 1.0
//
//  Total = min(dx,dy) × √2 + |dx - dy| × 1.0
//        = max(dx,dy) + (√2 - 1) × min(dx,dy)
//
//  This is EXACTLY the true cost in an open grid (no obstacles),
//  so it's the tightest possible admissible heuristic.
//  Tighter heuristic = fewer nodes expanded = faster search.
//
// ════════════════════════════════════════════════════════════════════════════════

double Pathfinding::octileHeuristic(int row, int col, int destRow, int destCol) {
    int dx = std::abs(col - destCol);
    int dy = std::abs(row - destRow);
    // √2 - 1 ≈ 0.41421356
    return std::max(dx, dy) + 0.41421356 * std::min(dx, dy);
}

// ════════════════════════════════════════════════════════════════════════════════
//  TRACE PATH
// ════════════════════════════════════════════════════════════════════════════════

std::vector<Pathfinding::Pos> Pathfinding::tracePath(
    const std::vector<std::vector<Pos>>& parents,
    int destRow, int destCol)
{
    std::vector<Pos> path;
    int row = destRow;
    int col = destCol;

    // Walk backwards: stop when a cell's parent is itself (the start cell)
    while (!(parents[row][col].first == row && parents[row][col].second == col)) {
        path.push_back({row, col});
        int prevRow = parents[row][col].first;
        int prevCol = parents[row][col].second;
        row = prevRow;
        col = prevCol;
    }
    // Add the start cell
    path.push_back({row, col});

    // Reverse: start → destination
    std::reverse(path.begin(), path.end());
    return path;
}

// ════════════════════════════════════════════════════════════════════════════════
//  A* SEARCH
// ════════════════════════════════════════════════════════════════════════════════

std::vector<Pathfinding::Pos> Pathfinding::findPath(
    int startRow, int startCol,
    int destRow, int destCol) const
{
    // Reset stats
    m_lastNodesExpanded = 0;
    m_lastPathCost = 0.0;

    // ── Validate ──
    if (!isValid(startRow, startCol) || !isValid(destRow, destCol)) {
        std::cout << "Pathfinding: start or destination out of bounds\n";
        return {};
    }
    if (!isPassable(startRow, startCol)) {
        std::cout << "Pathfinding: start (" << startRow << "," << startCol
                  << ") is blocked\n";
        return {};
    }
    if (!isPassable(destRow, destCol)) {
        std::cout << "Pathfinding: destination (" << destRow << "," << destCol
                  << ") is blocked\n";
        return {};
    }
    if (startRow == destRow && startCol == destCol) {
        return {{startRow, startCol}};
    }

    // ── Data structures ──
    const double INF = std::numeric_limits<double>::infinity();

    // g[r][c] = cheapest known cost from start to (r,c)
    std::vector<std::vector<double>> g(m_rows, std::vector<double>(m_cols, INF));

    // f[r][c] = g[r][c] + h(r,c)  (estimated total cost through this cell)
    std::vector<std::vector<double>> f(m_rows, std::vector<double>(m_cols, INF));

    // closed[r][c] = true if we've already finalized this cell
    std::vector<std::vector<bool>> closed(m_rows, std::vector<bool>(m_cols, false));

    // parent[r][c] = which cell we came from (for path reconstruction)
    std::vector<std::vector<Pos>> parent(m_rows, std::vector<Pos>(m_cols, {-1, -1}));

    // ── Initialize start ──
    g[startRow][startCol] = 0.0;
    f[startRow][startCol] = octileHeuristic(startRow, startCol, destRow, destCol);
    parent[startRow][startCol] = {startRow, startCol};  // start's parent is itself

    // ── Priority queue: (f_value, row, col) — min-heap ──
    using Node = std::tuple<double, int, int>;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> openList;
    openList.push({f[startRow][startCol], startRow, startCol});

    // ── Main loop ──
    while (!openList.empty()) {
        auto [curF, row, col] = openList.top();
        openList.pop();

        // Skip already-processed cells
        if (closed[row][col]) continue;
        closed[row][col] = true;
        m_lastNodesExpanded++;

        // ── Explore all 8 neighbors ──
        for (int d = 0; d < DIR_COUNT; d++) {
            int newRow = row + DR[d];
            int newCol = col + DC[d];

            if (!isPassable(newRow, newCol)) continue;
            if (closed[newRow][newCol]) continue;

            // Movement cost: 1.0 for cardinal, √2 for diagonal
            double newG = g[row][col] + MOVE_COST[d];

            // ── Found the destination ──
            if (newRow == destRow && newCol == destCol) {
                parent[newRow][newCol] = {row, col};
                m_lastPathCost = newG;
                return tracePath(parent, destRow, destCol);
            }

            // ── Update if this path is cheaper ──
            if (newG < g[newRow][newCol]) {
                g[newRow][newCol] = newG;
                f[newRow][newCol] = newG + octileHeuristic(newRow, newCol, destRow, destCol);
                parent[newRow][newCol] = {row, col};
                openList.push({f[newRow][newCol], newRow, newCol});
            }
        }
    }

    // ── No path found ──
    std::cout << "Pathfinding: no path from (" << startRow << "," << startCol
              << ") to (" << destRow << "," << destCol << ")\n";
    return {};
}