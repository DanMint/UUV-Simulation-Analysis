/**
 * test_pathfinding.cpp
 *
 * Tests the Pathfinding class for correct A* pathfinding,
 * obstacle avoidance, and edge cases.
 *
 * Exit codes:
 *   0 = PASS
 *   1 = FAIL
 *   2 = ERROR
 *
 * CMakeLists.txt registration:
 *
 * add_executable(test_pathfinding
 *     tests/test_pathfinding.cpp
 *     src/pathfinding/pathfinding.cpp
 *     src/mapCreation/mapCreation.cpp
 * )
 * target_include_directories(test_pathfinding PRIVATE
 *     src/pathfinding
 *     src/mapCreation
 * )
 * target_link_libraries(test_pathfinding PRIVATE
 *     SFML::Graphics SFML::Window SFML::System
 * )
 * if (GDAL_FOUND)
 *     target_link_libraries(test_pathfinding PRIVATE GDAL::GDAL)
 * else()
 *     target_include_directories(test_pathfinding PRIVATE "${GDAL_PREFIX}/include")
 *     target_link_directories(test_pathfinding PRIVATE ${GDAL_PREFIX}/lib")
 *     target_link_libraries(test_pathfinding PRIVATE gdal_i)
 * endif()
 * add_test(NAME test_pathfinding COMMAND test_pathfinding WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
 */

#include "pathfinding.h"
#include "mapCreation.h"

#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

// ════════════════════════════════════════════════════════════════════════════════
//  TEST HELPERS
// ════════════════════════════════════════════════════════════════════════════════

static int g_failures = 0;
static int g_checks = 0;

auto check = [&](const std::string& name, int actual, int expected) {
    g_checks++;
    bool pass = (actual == expected);
    std::cout << "  " << (pass ? "PASS" : "FAIL") << "  "
              << name << ": got " << actual << ", expected " << expected << "\n";
    if (!pass) g_failures++;
};

auto checkBool = [&](const std::string& name, bool actual, bool expected) {
    g_checks++;
    bool pass = (actual == expected);
    std::cout << "  " << (pass ? "PASS" : "FAIL") << "  "
              << name << ": got " << (actual ? "true" : "false")
              << ", expected " << (expected ? "true" : "false") << "\n";
    if (!pass) g_failures++;
};

auto checkGreater = [&](const std::string& name, int actual, int threshold) {
    g_checks++;
    bool pass = (actual > threshold);
    std::cout << "  " << (pass ? "PASS" : "FAIL") << "  "
              << name << ": got " << actual << ", expected > " << threshold << "\n";
    if (!pass) g_failures++;
};

// ════════════════════════════════════════════════════════════════════════════════
//  HELPER: create a simple grid
// ════════════════════════════════════════════════════════════════════════════════

static std::vector<std::vector<int>> makeGrid(int rows, int cols, int fill) {
    return std::vector<std::vector<int>>(rows, std::vector<int>(cols, fill));
}

// ════════════════════════════════════════════════════════════════════════════════
//  TESTS
// ════════════════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "=== test_pathfinding ===\n";

    // Test 1: Open water path
    {
        auto grid = makeGrid(5, 5, MapCreation::WATER);
        Pathfinding pf(grid);
        auto path = pf.findPath(0, 0, 4, 4);
        checkBool("open water path exists", !path.empty(), true);
        checkGreater("open water path length", static_cast<int>(path.size()), 0);
    }

    // Test 2: Same cell
    {
        auto grid = makeGrid(3, 3, MapCreation::WATER);
        Pathfinding pf(grid);
        auto path = pf.findPath(1, 1, 1, 1);
        checkBool("same cell path", !path.empty(), true);
    }

    // Test 3: Land obstacle avoidance
    {
        auto grid = makeGrid(5, 5, MapCreation::WATER);
        grid[2][2] = MapCreation::LAND;
        Pathfinding pf(grid);
        auto path = pf.findPath(0, 0, 4, 4);
        checkBool("path avoids land", !path.empty(), true);
        bool hitsLand = false;
        for (const auto& p : path) {
            if (grid[p.first][p.second] == MapCreation::LAND) { hitsLand = true; break; }
        }
        checkBool("path does not cross land", hitsLand, false);
    }

    // Test 4: Completely blocked
    {
        auto grid = makeGrid(3, 3, MapCreation::LAND);
        Pathfinding pf(grid);
        auto path = pf.findPath(0, 0, 2, 2);
        checkBool("blocked path empty", path.empty(), true);
    }

    // Test 5: Partial block
    {
        auto grid = makeGrid(5, 5, MapCreation::WATER);
        grid[2][1] = MapCreation::LAND;
        grid[2][2] = MapCreation::LAND;
        grid[2][3] = MapCreation::LAND;
        Pathfinding pf(grid);
        auto path = pf.findPath(0, 0, 4, 4);
        checkBool("partial block path exists", !path.empty(), true);
    }

    // Test 6: Diagonal movement
    {
        auto grid = makeGrid(5, 5, MapCreation::WATER);
        Pathfinding pf(grid);
        auto path = pf.findPath(0, 0, 4, 4);
        checkBool("diagonal path exists", !path.empty(), true);
    }

    // Test 7: Large open map
    {
        auto grid = makeGrid(50, 50, MapCreation::WATER);
        Pathfinding pf(grid);
        auto path = pf.findPath(0, 0, 49, 49);
        checkBool("large map path exists", !path.empty(), true);
        checkGreater("large map path length", static_cast<int>(path.size()), 0);
    }

    // Test 8: Maze-like grid
    {
        auto grid = makeGrid(7, 7, MapCreation::LAND);
        for (int r = 0; r < 7; ++r) grid[r][0] = MapCreation::WATER;
        for (int r = 0; r < 7; ++r) grid[r][6] = MapCreation::WATER;
        for (int c = 0; c < 7; ++c) grid[0][c] = MapCreation::WATER;
        for (int c = 0; c < 7; ++c) grid[6][c] = MapCreation::WATER;
        grid[1][1] = MapCreation::WATER;
        grid[2][2] = MapCreation::WATER;
        grid[3][3] = MapCreation::WATER;
        grid[4][4] = MapCreation::WATER;
        grid[5][5] = MapCreation::WATER;
        Pathfinding pf(grid);
        auto path = pf.findPath(1, 1, 5, 5);
        checkBool("maze path exists", !path.empty(), true);
    }

    // Summary
    std::cout << "\n=== Results: " << g_checks << " checks, "
              << (g_checks - g_failures) << " passed, "
              << g_failures << " failed ===\n";

    return g_failures > 0 ? 1 : 0;
}
