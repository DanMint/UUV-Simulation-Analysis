#ifndef SPATIAL_GRID_H
#define SPATIAL_GRID_H

#include <vector>
#include <unordered_map>
#include <cstdint>

/**
 * SpatialGrid — Coarse spatial hash for O(1) range queries on a 2D grid.
 *
 * Instead of O(N*M) nested loops for "find all agents within radius R",
 * the grid partitions space into cells of size `cellSize`. To find all
 * agents near (r,c), we only check agents in the same cell and adjacent
 * cells (at most 9 cells for 8-connected neighborhoods).
 *
 * Usage:
 *   SpatialGrid grid(50.0);  // 50-cell buckets
 *   grid.insert(0, 100, 200);  // agent 0 at (100, 200)
 *   grid.insert(1, 105, 205);
 *   auto nearby = grid.query(102, 203, 10.0);  // agents within 10 units of (102,203)
 *
 * Thread safety: Not thread-safe. Use one instance per simulation step
 * or protect with a mutex.
 */
class SpatialGrid {
public:
    using CellKey = int64_t;  // packed (row, col) for hash key

    /**
     * @param cellSize  Width/height of each spatial cell (same units as row/col).
     */
    explicit SpatialGrid(double cellSize)
        : m_cellSize(cellSize), m_invCellSize(1.0 / cellSize) {}

    /** Clear all agents from the grid. Call once per simulation step. */
    void clear() { m_cells.clear(); }

    /** Insert an agent at (row, col) with ID `id`. */
    void insert(int id, int row, int col) {
        CellKey key = cellKey(row, col);
        m_cells[key].push_back(id);
    }

    /**
     * Query all agent IDs within `radius` of (row, col).
     * Returns a vector of agent IDs from nearby cells (caller must
     * perform exact distance filtering if needed).
     */
    std::vector<int> query(int row, int col, double radius) const {
        std::vector<int> result;
        const int minR = cellCoord(static_cast<int>(row - radius));
        const int maxR = cellCoord(static_cast<int>(row + radius));
        const int minC = cellCoord(static_cast<int>(col - radius));
        const int maxC = cellCoord(static_cast<int>(col + radius));

        for (int cr = minR; cr <= maxR; ++cr) {
            for (int cc = minC; cc <= maxC; ++cc) {
                auto it = m_cells.find(pack(cr, cc));
                if (it != m_cells.end()) {
                    const auto& ids = it->second;
                    result.insert(result.end(), ids.begin(), ids.end());
                }
            }
        }
        return result;
    }

    /** Get the cell size used by this grid. */
    double cellSize() const { return m_cellSize; }

private:
    double m_cellSize;
    double m_invCellSize;
    std::unordered_map<CellKey, std::vector<int>> m_cells;

    static inline CellKey pack(int row, int col) noexcept {
        return (static_cast<CellKey>(row) << 32) | static_cast<uint32_t>(col);
    }

    inline CellKey cellKey(int row, int col) const noexcept {
        return pack(cellCoord(row), cellCoord(col));
    }

    inline int cellCoord(int coord) const noexcept {
        if (coord >= 0) {
            return static_cast<int>(coord * m_invCellSize);
        } else {
            return -static_cast<int>((-coord + static_cast<int>(m_cellSize) - 1) * m_invCellSize);
        }
    }
};

#endif // SPATIAL_GRID_H
