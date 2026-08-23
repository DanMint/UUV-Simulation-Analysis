/**
 * test_mapCreation.cpp
 *
 * Tests the MapCreation class for correct grid parsing,
 * terrain queries, and boundary handling.
 *
 * Exit codes:
 *   0 = PASS
 *   1 = FAIL
 *   2 = ERROR
 *
 * CMakeLists.txt registration:
 *
 * add_executable(test_mapCreation
 *     tests/test_mapCreation.cpp
 *     src/mapCreation/mapCreation.cpp
 *     src/pathfinding/pathfinding.cpp
 * )
 * target_include_directories(test_mapCreation PRIVATE
 *     src/mapCreation
 *     src/pathfinding
 * )
 * target_link_libraries(test_mapCreation PRIVATE
 *     SFML::Graphics SFML::Window SFML::System
 * )
 * if (GDAL_FOUND)
 *     target_link_libraries(test_mapCreation PRIVATE GDAL::GDAL)
 * else()
 *     target_include_directories(test_mapCreation PRIVATE "${GDAL_PREFIX}/include")
 *     target_link_directories(test_mapCreation PRIVATE ${GDAL_PREFIX}/lib")
 *     target_link_libraries(test_mapCreation PRIVATE gdal_i)
 * endif()
 * add_test(NAME test_mapCreation COMMAND test_mapCreation WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
 */

#include "mapCreation.h"
#include "pathfinding.h"

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
//  TESTS
// ════════════════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "=== test_mapCreation ===\n";

    // Test 1: Default constructor
    {
        MapCreation map;
        check("default width", static_cast<int>(map.getGrid().size()), 0);
        check("default height", map.getGrid().empty() ? 0 : static_cast<int>(map.getGrid()[0].size()), 0);
    }

    // Test 2: fromGridData with simple map
    {
        std::vector<std::vector<int>> grid = {
            {MapCreation::WATER, MapCreation::WATER, MapCreation::LAND},
            {MapCreation::LAND, MapCreation::WATER, MapCreation::WATER},
            {MapCreation::WATER, MapCreation::LAND, MapCreation::WATER}
        };
        MapCreation map = MapCreation::fromGridData(grid, 300, 300);
        check("fromGridData width", static_cast<int>(map.getGrid().size()), 3);
        check("fromGridData height", static_cast<int>(map.getGrid()[0].size()), 3);
    }

    // Test 3: isWater and getCell
    {
        std::vector<std::vector<int>> grid = {
            {MapCreation::WATER, MapCreation::LAND},
            {MapCreation::LAND, MapCreation::WATER}
        };
        MapCreation map = MapCreation::fromGridData(grid, 200, 200);
        checkBool("isWater(0,0)", map.isWater(0, 0), true);
        check("getCell(0,0)", map.getCell(0, 0), MapCreation::WATER);
        checkBool("getCell(0,1) is LAND", map.getCell(0, 1) == MapCreation::LAND, true);
        checkBool("getCell(1,0) is LAND", map.getCell(1, 0) == MapCreation::LAND, true);
        checkBool("isWater(1,1)", map.isWater(1, 1), true);
    }

    // Test 4: getCell
    {
        std::vector<std::vector<int>> grid = {
            {MapCreation::WATER, MapCreation::LAND}
        };
        MapCreation map = MapCreation::fromGridData(grid, 200, 200);
        check("getCell(0,0)", map.getCell(0, 0), MapCreation::WATER);
        check("getCell(0,1)", map.getCell(0, 1), MapCreation::LAND);
    }

    // Test 5: All water map
    {
        std::vector<std::vector<int>> grid(5, std::vector<int>(5, MapCreation::WATER));
        MapCreation map = MapCreation::fromGridData(grid, 500, 500);
        checkBool("all water isWater", map.isWater(2, 2), true);
        check("all water getCell", map.getCell(2, 2), MapCreation::WATER);
    }

    // Test 6: All land map
    {
        std::vector<std::vector<int>> grid(5, std::vector<int>(5, MapCreation::LAND));
        MapCreation map = MapCreation::fromGridData(grid, 500, 500);
        checkBool("all land isWater", map.isWater(2, 2), false);
        check("all land getCell", map.getCell(2, 2), MapCreation::LAND);
    }

    // Test 7: Pathfinding integration
    {
        std::vector<std::vector<int>> grid = {
            {MapCreation::WATER, MapCreation::WATER, MapCreation::WATER},
            {MapCreation::WATER, MapCreation::LAND, MapCreation::WATER},
            {MapCreation::WATER, MapCreation::WATER, MapCreation::WATER}
        };
        MapCreation map = MapCreation::fromGridData(grid, 300, 300);
        Pathfinding pf(map.getGrid());
        auto path = pf.findPath(0, 0, 2, 2);
        checkBool("pathfinding around land", !path.empty(), true);
    }

    // Test 8: Large map
    {
        std::vector<std::vector<int>> grid(100, std::vector<int>(100, MapCreation::WATER));
        MapCreation map = MapCreation::fromGridData(grid, 1000, 1000);
        check("large map width", static_cast<int>(map.getGrid().size()), 100);
        check("large map height", static_cast<int>(map.getGrid()[0].size()), 100);
        checkBool("large map isWater", map.isWater(50, 50), true);
    }

    // Summary
    std::cout << "\n=== Results: " << g_checks << " checks, "
              << (g_checks - g_failures) << " passed, "
              << g_failures << " failed ===\n";

    return g_failures > 0 ? 1 : 0;
}
