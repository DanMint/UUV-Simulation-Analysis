/**
 * @file test_spatialGrid.cpp
 * @brief Unit tests for the SpatialGrid utility.
 */

#include "../src/utils/spatialGrid.h"
#include <cmath>
#include <cstdio>
#include <cstring>

static int g_failures = 0;

static void check(bool cond, const char* name) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", name);
        g_failures++;
    }
}

int main() {
    // Test 1: Basic insert and query
    {
        SpatialGrid grid(10.0);
        grid.insert(0, 0, 0);
        grid.insert(1, 5, 5);
        grid.insert(2, 100, 100);

        auto nearby = grid.query(0, 0, 15.0);
        check(nearby.size() == 2, "query_radius_15_finds_2");
        check(std::find(nearby.begin(), nearby.end(), 0) != nearby.end(), "query_finds_id_0");
        check(std::find(nearby.begin(), nearby.end(), 1) != nearby.end(), "query_finds_id_1");
    }

    // Test 2: Query with small radius - agents in same cell may be returned
    // (spatial grid returns cell contents; caller filters by exact distance)
    {
        SpatialGrid grid(10.0);
        grid.insert(0, 0, 0);
        grid.insert(1, 9, 9);
        auto nearby = grid.query(0, 0, 5.0);
        // Both in same cell (0,0), grid returns both; distance filter done by caller
        check(nearby.size() == 2, "query_same_cell_returns_both");
    }

    // Test 3: Clear resets grid
    {
        SpatialGrid grid(10.0);
        grid.insert(0, 0, 0);
        grid.clear();
        auto nearby = grid.query(0, 0, 15.0);
        check(nearby.empty(), "clear_removes_all");
    }

    // Test 4: Multiple agents in same cell
    {
        SpatialGrid grid(10.0);
        grid.insert(0, 5, 5);
        grid.insert(1, 5, 5);
        grid.insert(2, 5, 5);
        auto nearby = grid.query(5, 5, 5.0);
        check(nearby.size() == 3, "same_cell_3_agents");
    }

    // Test 5: Large grid
    {
        SpatialGrid grid(50.0);
        for (int i = 0; i < 1000; i++) {
            grid.insert(i, i * 3, i * 7);
        }
        auto nearby = grid.query(150, 350, 60.0);
        check(!nearby.empty(), "large_grid_query_returns_results");
    }

    // Test 6: Negative coordinates
    {
        SpatialGrid grid(10.0);
        grid.insert(0, -5, -5);
        grid.insert(1, 5, 5);
        auto nearby = grid.query(-5, -5, 15.0);
        // Both in same cell (0,0), distance from (-5,-5) to (5,5) ≈ 14.14 < 15
        check(nearby.size() == 2, "negative_coords_work");
    }

    std::fprintf(stdout, "\n[TEST] %d checks, %d failures\n", 6, g_failures);
    std::fflush(stdout);
    return g_failures == 0 ? 0 : 1;
}
