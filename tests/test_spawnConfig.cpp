/**
 * test_spawnConfig.cpp
 *
 * Tests the SpawnConfig class for correct parsing,
 * unit management, and configuration validation.
 *
 * Exit codes:
 *   0 = PASS
 *   1 = FAIL
 *   2 = ERROR
 *
 * CMakeLists.txt registration:
 *
 * add_executable(test_spawnConfig
 *     tests/test_spawnConfig.cpp
 *     src/spawnConfig/spawnConfig.cpp
 * )
 * target_include_directories(test_spawnConfig PRIVATE
 *     src/spawnConfig
 * )
 * add_test(NAME test_spawnConfig COMMAND test_spawnConfig WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
 */

#include "spawnConfig.h"

#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <cmath>

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

auto checkDouble = [&](const std::string& name, double actual, double expected) {
    g_checks++;
    bool pass = (std::abs(actual - expected) < 1e-9);
    std::cout << "  " << (pass ? "PASS" : "FAIL") << "  "
              << name << ": got " << actual << ", expected " << expected << "\n";
    if (!pass) g_failures++;
};

// ════════════════════════════════════════════════════════════════════════════════
//  TESTS
// ════════════════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "=== test_spawnConfig ===\n";

    // Test 1: Default constructor
    {
        SpawnConfig config;
        checkDouble("default maxNoiseLevel", config.getMaxNoiseLevel(), 0.0);
        checkDouble("default detectorRadius", config.getDetectorRadius(), 3.0);
        checkDouble("default interceptorRadius", config.getInterceptorRadius(), 3.0);
        check("default units size", static_cast<int>(config.getUnits().size()), 0);
    }

    // Test 2: Add units
    {
        SpawnConfig config;
        config.addUnit("seeker", 0, 0, "bluerov2");
        config.addUnit("target", 5, 5, "", true);
        config.addUnit("detector", 2, 2);
        check("units size after add", static_cast<int>(config.getUnits().size()), 3);
    }

    // Test 3: Set noise level
    {
        SpawnConfig config;
        config.setMaxNoiseLevel(0.05);
        checkDouble("maxNoiseLevel after set", config.getMaxNoiseLevel(), 0.05);
    }

    // Test 4: Set radii
    {
        SpawnConfig config;
        config.setDetectorRadius(15.0);
        config.setInterceptorRadius(10.0);
        checkDouble("detectorRadius", config.getDetectorRadius(), 15.0);
        checkDouble("interceptorRadius", config.getInterceptorRadius(), 10.0);
    }

    // Test 5: Unit type filtering
    {
        SpawnConfig config;
        config.addUnit("seeker", 0, 0, "bluerov2");
        config.addUnit("seeker", 1, 1, "riptide");
        config.addUnit("target", 5, 5, "", true);
        config.addUnit("detector", 2, 2);
        config.addUnit("interceptor", 3, 3);
        config.addUnit("attacker", 4, 4, "bluerov2");

        int seekers = 0, targets = 0, detectors = 0, interceptors = 0, attackers = 0;
        for (const auto& u : config.getUnits()) {
            if (u.type == "seeker") seekers++;
            else if (u.type == "target") targets++;
            else if (u.type == "detector") detectors++;
            else if (u.type == "interceptor") interceptors++;
            else if (u.type == "attacker") attackers++;
        }
        check("seeker count", seekers, 2);
        check("target count", targets, 1);
        check("detector count", detectors, 1);
        check("interceptor count", interceptors, 1);
        check("attacker count", attackers, 1);
    }

    // Test 6: Vehicle type storage
    {
        SpawnConfig config;
        config.addUnit("seeker", 0, 0, "bluerov2");
        config.addUnit("attacker", 4, 4, "riptide");
        checkBool("seeker vehicleType", config.getUnits()[0].vehicleType == "bluerov2", true);
        checkBool("attacker vehicleType", config.getUnits()[1].vehicleType == "riptide", true);
    }

    // Test 7: Critical asset flag
    {
        SpawnConfig config;
        config.addUnit("target", 0, 0, "", true);
        config.addUnit("target", 5, 5, "", false);
        checkBool("critical target", config.getUnits()[0].isCritical, true);
        checkBool("non-critical target", config.getUnits()[1].isCritical, false);
    }

    // Summary
    std::cout << "\n=== Results: " << g_checks << " checks, "
              << (g_checks - g_failures) << " passed, "
              << g_failures << " failed ===\n";

    return g_failures > 0 ? 1 : 0;
}
