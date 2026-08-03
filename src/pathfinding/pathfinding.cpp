#include "pathfinding.h"

#include "mapCreation.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <queue>
#include <tuple>

// ════════════════════════════════════════════════════════════════════════════════
//  CONSTRUCTOR
// ════════════════════════════════════════════════════════════════════════════════

Pathfinding::Pathfinding(const std::vector<std::vector<int>>& grid)
    : m_grid(grid),
      m_rows(static_cast<int>(grid.size())),
      m_cols(m_rows > 0 ? static_cast<int>(grid[0].size()) : 0),
      m_lastNodesExpanded(0),
      m_lastPathCost(0.0)
{
}

// ════════════════════════════════════════════════════════════════════════════════
//  A* SEARCH
// ════════════════════════════════════════════════════════════════════════════════

std::vector<Pathfinding::Pos> Pathfinding::findPath(
    int startRow,
    int startCol,
    int destRow,
    int destCol
) const {
    m_lastNodesExpanded = 0;
    m_lastPathCost = 0.0;

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

    const double infinity = std::numeric_limits<double>::infinity();

    std::vector<std::vector<double>> g(
        m_rows,
        std::vector<double>(m_cols, infinity)
    );

    std::vector<std::vector<double>> f(
        m_rows,
        std::vector<double>(m_cols, infinity)
    );

    std::vector<std::vector<bool>> closed(
        m_rows,
        std::vector<bool>(m_cols, false)
    );

    std::vector<std::vector<Pos>> parent(
        m_rows,
        std::vector<Pos>(m_cols, {-1, -1})
    );

    g[startRow][startCol] = 0.0;
    f[startRow][startCol] = octileHeuristic(
        startRow,
        startCol,
        destRow,
        destCol
    );
    parent[startRow][startCol] = {startRow, startCol};

    using Node = std::tuple<double, int, int>;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> openList;
    openList.push({f[startRow][startCol], startRow, startCol});

    while (!openList.empty()) {
        const auto [currentF, row, col] = openList.top();
        (void)currentF;
        openList.pop();

        if (closed[row][col]) {
            continue;
        }

        closed[row][col] = true;
        ++m_lastNodesExpanded;

        for (int direction = 0; direction < DIR_COUNT; ++direction) {
            const int newRow = row + DR[direction];
            const int newCol = col + DC[direction];

            if (!isPassable(newRow, newCol)) {
                continue;
            }

            if (closed[newRow][newCol]) {
                continue;
            }

            const double newG = g[row][col] + MOVE_COST[direction];

            if (newRow == destRow && newCol == destCol) {
                parent[newRow][newCol] = {row, col};
                m_lastPathCost = newG;
                return tracePath(parent, destRow, destCol);
            }

            if (newG < g[newRow][newCol]) {
                g[newRow][newCol] = newG;
                f[newRow][newCol] = newG + octileHeuristic(
                    newRow,
                    newCol,
                    destRow,
                    destCol
                );
                parent[newRow][newCol] = {row, col};
                openList.push({f[newRow][newCol], newRow, newCol});
            }
        }
    }

    std::cout << "Pathfinding: no path from ("
              << startRow << "," << startCol << ") to ("
              << destRow << "," << destCol << ")\n";

    return {};
}

// ════════════════════════════════════════════════════════════════════════════════
//  HELPERS
// ════════════════════════════════════════════════════════════════════════════════

bool Pathfinding::isValid(int row, int col) const {
    return row >= 0 && row < m_rows && col >= 0 && col < m_cols;
}

bool Pathfinding::isPassable(int row, int col) const {
    // The grid stores broad category IDs only. All categories are passable;
    // land is the sole blocked value.
    return isValid(row, col) && m_grid[row][col] != MapCreation::LAND;
}

int Pathfinding::getLastNodesExpanded() const {
    return m_lastNodesExpanded;
}

double Pathfinding::getLastPathCost() const {
    return m_lastPathCost;
}

// ════════════════════════════════════════════════════════════════════════════════
//  OCTILE HEURISTIC
// ════════════════════════════════════════════════════════════════════════════════

double Pathfinding::octileHeuristic(
    int row,
    int col,
    int destRow,
    int destCol
) {
    const int dx = std::abs(col - destCol);
    const int dy = std::abs(row - destRow);

    return std::max(dx, dy) + 0.41421356 * std::min(dx, dy);
}

// ════════════════════════════════════════════════════════════════════════════════
//  TRACE PATH
// ════════════════════════════════════════════════════════════════════════════════

std::vector<Pathfinding::Pos> Pathfinding::tracePath(
    const std::vector<std::vector<Pos>>& parents,
    int destRow,
    int destCol
) {
    std::vector<Pos> path;
    int row = destRow;
    int col = destCol;

    while (!(parents[row][col].first == row &&
             parents[row][col].second == col)) {
        path.push_back({row, col});

        const int previousRow = parents[row][col].first;
        const int previousCol = parents[row][col].second;
        row = previousRow;
        col = previousCol;
    }

    path.push_back({row, col});
    std::reverse(path.begin(), path.end());
    return path;
}