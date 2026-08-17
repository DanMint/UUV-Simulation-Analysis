#include "pathfinding.h"

#include <cmath>
#include <limits>
#include <algorithm>
#include <iostream>
#include <cstring>

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
    const int total = m_rows * m_cols;

    // Pre-allocate flat buffers once (reused across all findPath calls)
    m_gFlat.resize(total);
    m_fFlat.resize(total);
    m_closedFlat.resize(total);
    m_parentFlat.resize(total);
    m_openFlat.reserve(total);
}

// ════════════════════════════════════════════════════════════════════════════════
//  HELPERS
// ════════════════════════════════════════════════════════════════════════════════

bool Pathfinding::isValid(int row, int col) const {
    return row >= 0 && row < m_rows && col >= 0 && col < m_cols;
}

bool Pathfinding::isPassable(int row, int col) const {
    return isValid(row, col) && m_grid[row][col] != 1;
}

int Pathfinding::getLastNodesExpanded() const { return m_lastNodesExpanded; }
double Pathfinding::getLastPathCost() const { return m_lastPathCost; }

// ════════════════════════════════════════════════════════════════════════════════
//  OCTILE HEURISTIC
// ════════════════════════════════════════════════════════════════════════════════

double Pathfinding::octileHeuristic(int row, int col, int destRow, int destCol) {
    int dx = std::abs(col - destCol);
    int dy = std::abs(row - destRow);
    return std::max(dx, dy) + 0.41421356 * std::min(dx, dy);
}

// ════════════════════════════════════════════════════════════════════════════════
//  BINARY HEAP (flat array, avoids priority_queue allocations)
// ════════════════════════════════════════════════════════════════════════════════

void Pathfinding::heapPush(double f, int nodeIdx) const {
    m_openFlat.emplace_back(f, nodeIdx);
    int child = static_cast<int>(m_openFlat.size()) - 1;
    while (child > 0) {
        int parent = (child - 1) / 2;
        if (m_openFlat[parent].first <= m_openFlat[child].first) break;
        auto tmp = m_openFlat[parent]; m_openFlat[parent] = m_openFlat[child]; m_openFlat[child] = tmp;
        child = parent;
    }
}

std::pair<double, int> Pathfinding::heapPop() const {
    std::pair<double, int> top = m_openFlat.front();
    const int last = static_cast<int>(m_openFlat.size()) - 1;
    m_openFlat[0] = m_openFlat[last];
    m_openFlat.pop_back();

    int parent = 0;
    while (true) {
        int left = 2 * parent + 1;
        int right = left + 1;
        int smallest = parent;
        if (left < static_cast<int>(m_openFlat.size()) && m_openFlat[left].first < m_openFlat[smallest].first)
            smallest = left;
        if (right < static_cast<int>(m_openFlat.size()) && m_openFlat[right].first < m_openFlat[smallest].first)
            smallest = right;
        if (smallest == parent) break;
        auto tmp = m_openFlat[parent]; m_openFlat[parent] = m_openFlat[smallest]; m_openFlat[smallest] = tmp;
        parent = smallest;
    }
    return top;
}

bool Pathfinding::heapEmpty() const {
    return m_openFlat.empty();
}

// ════════════════════════════════════════════════════════════════════════════════
//  TRACE PATH (flat parent array)
// ════════════════════════════════════════════════════════════════════════════════

std::vector<Pathfinding::Pos> Pathfinding::tracePath(int destRow, int destCol) const {
    std::vector<Pos> path;
    int destIdx = idx(destRow, destCol, m_cols);

    int cur = destIdx;
    while (true) {
        path.push_back({rowFromIdx(cur, m_cols), colFromIdx(cur, m_cols)});
        if (m_parentFlat[cur].first == cur) break;
        cur = m_parentFlat[cur].first;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

// ════════════════════════════════════════════════════════════════════════════════
//  A* SEARCH (flat-buffer implementation)
// ════════════════════════════════════════════════════════════════════════════════

std::vector<Pathfinding::Pos> Pathfinding::findPath(
    int startRow, int startCol,
    int destRow, int destCol) const
{
    // Reset stats
    m_lastNodesExpanded = 0;
    m_lastPathCost = 0.0;

    if (!isValid(startRow, startCol) || !isValid(destRow, destCol)) {
        return {};
    }
    if (!isPassable(startRow, startCol)) {
        return {};
    }
    if (!isPassable(destRow, destCol)) {
        return {};
    }
    if (startRow == destRow && startCol == destCol) {
        return {{startRow, startCol}};
    }

    const int total = m_rows * m_cols;
    const double INF = std::numeric_limits<double>::infinity();

    // Zero-initialize flat buffers (no allocations)
    std::fill(m_gFlat.begin(), m_gFlat.end(), INF);
    std::fill(m_fFlat.begin(), m_fFlat.end(), INF);
    std::memset(m_closedFlat.data(), 0, total);
    std::fill(m_parentFlat.begin(), m_parentFlat.end(), Pos{-1, -1});
    m_openFlat.clear();

    const int startIdx = idx(startRow, startCol, m_cols);
    const int destIdx = idx(destRow, destCol, m_cols);

    m_gFlat[startIdx] = 0.0;
    m_fFlat[startIdx] = octileHeuristic(startRow, startCol, destRow, destCol);
    m_parentFlat[startIdx] = {startIdx, startIdx};  // (parent, node)
    heapPush(m_fFlat[startIdx], startIdx);

    static constexpr int DIR_COUNT = 8;
    static constexpr int DR[DIR_COUNT] = {-1,-1,-1, 0, 0, 1, 1, 1};
    static constexpr int DC[DIR_COUNT] = {-1, 0, 1,-1, 1,-1, 0, 1};
    static constexpr double MOVE_COST[DIR_COUNT] = {
        1.41421356, 1.0, 1.41421356,
        1.0, 1.0,
        1.41421356, 1.0, 1.41421356
    };

    while (!heapEmpty()) {
        auto [curF, nodeIdx] = heapPop();
        const int row = rowFromIdx(nodeIdx, m_cols);
        const int col = colFromIdx(nodeIdx, m_cols);

        if (m_closedFlat[nodeIdx]) continue;
        m_closedFlat[nodeIdx] = 1;
        m_lastNodesExpanded++;

        if (nodeIdx == destIdx) {
            m_lastPathCost = m_gFlat[nodeIdx];
            return tracePath(destRow, destCol);
        }

        for (int d = 0; d < DIR_COUNT; d++) {
            const int nr = row + DR[d];
            const int nc = col + DC[d];
            if (!isPassable(nr, nc)) continue;
            const int ni = idx(nr, nc, m_cols);
            if (m_closedFlat[ni]) continue;

            const double newG = m_gFlat[nodeIdx] + MOVE_COST[d];

            if (newG < m_gFlat[ni]) {
                m_gFlat[ni] = newG;
                m_fFlat[ni] = newG + octileHeuristic(nr, nc, destRow, destCol);
                m_parentFlat[ni] = {nodeIdx, ni};
                heapPush(m_fFlat[ni], ni);
            }
        }
    }

    return {};
}
