/**
 * test_simulationRecorder.cpp
 *
 * Tests the SimulationRecorder class for correct step recording,
 * event filtering, agent statistics, JSON/binary serialization,
 * and state management.
 *
 * Exit codes:
 *   0 = PASS
 *   1 = FAIL
 *   2 = ERROR
 *
 * CMakeLists.txt registration:
 *
 * add_executable(test_simulationRecorder
 *     tests/test_simulationRecorder.cpp
 *     src/mapCreation/mapCreation.cpp
 *     src/spawnConfig/spawnConfig.cpp
 *     src/simulation/simulation.cpp
 *     src/simulation/simulationRecorder.cpp
 *     src/simulation/simResult.cpp
 *     src/pathfinding/pathfinding.cpp
 *     src/agents/targetAgent.cpp
 *     src/agents/seekerAgent.cpp
 *     src/agents/attackerAgent.cpp
 *     src/agents/detectorAgent.cpp
 *     src/agents/interceptorAgent.cpp
 *     src/agents/vehicleSpecs.cpp
 * )
 * target_include_directories(test_simulationRecorder PRIVATE
 *     src/agents
 *     src/mapCreation
 *     src/spawnConfig
 *     src/pathfinding
 *     src/simulation
 *     src/utils
 * )
 * target_link_libraries(test_simulationRecorder PRIVATE
 *     SFML::Graphics SFML::Window SFML::System
 * )
 * if (GDAL_FOUND)
 *     target_link_libraries(test_simulationRecorder PRIVATE GDAL::GDAL)
 * else()
 *     target_include_directories(test_simulationRecorder PRIVATE "${GDAL_PREFIX}/include")
 *     target_link_directories(test_simulationRecorder PRIVATE "${GDAL_PREFIX}/lib")
 *     target_link_libraries(test_simulationRecorder PRIVATE gdal_i)
 * endif()
 * add_test(NAME test_simulationRecorder COMMAND test_simulationRecorder WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
 */

#include "simulationRecorder.h"
#include "simulation.h"
#include "mapCreation.h"
#include "spawnConfig.h"
#include "pathfinding.h"
#include "seekerAgent.h"
#include "targetAgent.h"
#include "detectorAgent.h"
#include "interceptorAgent.h"
#include "attackerAgent.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cstring>

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

auto checkSizeT = [&](const std::string& name, size_t actual, size_t expected) {
    g_checks++;
    bool pass = (actual == expected);
    std::cout << "  " << (pass ? "PASS" : "FAIL") << "  "
              << name << ": got " << actual << ", expected " << expected << "\n";
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
//  FIXTURE: 3x3 grid map with minimal spawn config
// ════════════════════════════════════════════════════════════════════════════════

struct TestFixture {
    MapCreation map;
    SpawnConfig config;
    static constexpr int GRID_SIZE = 10;

    TestFixture() {
        std::vector<std::vector<int>> grid(GRID_SIZE, std::vector<int>(GRID_SIZE, MapCreation::WATER));

        MapInfo info;
        info.cellsN = GRID_SIZE;
        info.canvasWidth = GRID_SIZE * 100;
        info.canvasHeight = GRID_SIZE * 100;
        info.width = GRID_SIZE;
        info.height = GRID_SIZE;
        info.waterCount = GRID_SIZE * GRID_SIZE;
        info.landCount = 0;
        info.minDepth = 1.0;
        info.maxDepth = 10.0;

        config.setMapData(info, grid);

        config.addUnit("seeker", 0, 0);
        config.addUnit("target", 9, 9);
        config.addUnit("detector", 5, 5);
        config.addUnit("interceptor", 0, 9);
        config.addUnit("attacker", 9, 0);

        config.setDetectorRadius(3.0);
        config.setInterceptorRadius(3.0);
        config.setMaxNoiseLevel(0.0);

        map = MapCreation::fromGridData(grid, GRID_SIZE, GRID_SIZE * 100, GRID_SIZE * 100);

        for (const auto& unit : config.getUnits()) {
            int t = MapCreation::WATER;
            if (unit.type == "seeker")       t = MapCreation::SEEKER;
            else if (unit.type == "target")  t = MapCreation::TARGET;
            else if (unit.type == "detector")   t = MapCreation::DETECTOR;
            else if (unit.type == "interceptor") t = MapCreation::INTERCEPTOR;
            else if (unit.type == "attacker")   t = MapCreation::ATTACKER;
            if (t != MapCreation::WATER) map.placeUnit(unit.row, unit.col, t);
        }
    }
};

// ════════════════════════════════════════════════════════════════════════════════
//  MAIN
// ════════════════════════════════════════════════════════════════════════════════

int main() {
    try {
        TestFixture fixture;

        // ── Test 1: Constructor and metadata ─────────────────────────────
        std::cout << "\n[TEST] === Constructor and Metadata ===\n";
        {
            SimulationRecorder::RunMetadata meta{};
            meta.runId = 42;
            meta.scenarioName = "test_scenario";
            meta.mapHash = "abc123";
            meta.maxSteps = 100;
            meta.noiseLevel = 0.5;
            meta.seed = 12345;
            meta.startTime = "2026-01-01T00:00:00Z";
            meta.wallTimeMs = 12.5;
            meta.totalSeekers = 1;
            meta.totalTargets = 1;
            meta.totalDetectors = 1;
            meta.totalInterceptors = 1;
            meta.totalAttackers = 1;

            SimulationRecorder recorder(meta);

            check("metadata.runId", recorder.metadata().runId, 42);
            check("metadata.maxSteps", recorder.metadata().maxSteps, 100);
            check("metadata.totalSeekers", recorder.metadata().totalSeekers, 1);
            check("metadata.totalTargets", recorder.metadata().totalTargets, 1);
            check("metadata.totalDetectors", recorder.metadata().totalDetectors, 1);
            check("metadata.totalInterceptors", recorder.metadata().totalInterceptors, 1);
            check("metadata.totalAttackers", recorder.metadata().totalAttackers, 1);
            checkBool("hasData initially", recorder.hasData(), false);
            checkSizeT("stepCount initially", recorder.stepCount(), 0);
        }

        // ── Test 2: recordStep() records correct number of steps ─────────
        std::cout << "\n[TEST] === recordStep() ===\n";
        {
            SimulationRecorder::RunMetadata meta{};
            meta.runId = 1;
            meta.scenarioName = "test";
            meta.mapHash = "hash";
            meta.maxSteps = 10;
            meta.noiseLevel = 0.0;
            meta.seed = 99;
            meta.startTime = SimulationRecorder::timestampNow();
            meta.wallTimeMs = 0.0;
            meta.totalSeekers = 1;
            meta.totalTargets = 1;
            meta.totalDetectors = 1;
            meta.totalInterceptors = 1;
            meta.totalAttackers = 1;

            SimulationRecorder recorder(meta);

            Simulation sim(fixture.map, fixture.config, 10, 99);
            sim.setRecorder(&recorder);

            for (int i = 0; i < 5; i++) {
                if (sim.isFinished()) break;
                sim.stepOnce();
            }

            checkGreater("stepCount after stepping", static_cast<int>(recorder.stepCount()), 0);
            checkBool("hasData after recording", recorder.hasData(), true);
        }

        // ── Test 3: recordEvent() records events correctly ───────────────
        std::cout << "\n[TEST] === recordEvent() ===\n";
        {
            SimulationRecorder::RunMetadata meta{};
            meta.runId = 2;
            meta.scenarioName = "test";
            meta.mapHash = "hash";
            meta.maxSteps = 10;
            meta.noiseLevel = 0.0;
            meta.seed = 99;
            meta.startTime = SimulationRecorder::timestampNow();
            meta.wallTimeMs = 0.0;
            meta.totalSeekers = 1;
            meta.totalTargets = 1;
            meta.totalDetectors = 1;
            meta.totalInterceptors = 1;
            meta.totalAttackers = 1;

            SimulationRecorder recorder(meta);

            recorder.recordEvent(0, SimulationRecorder::EVENT_DETECTION, 0, 0);
            recorder.recordEvent(1, SimulationRecorder::EVENT_INTERCEPT, 0, 1);
            recorder.recordEvent(2, SimulationRecorder::EVENT_TARGET_DESTROYED, 1, 0);

            auto events = recorder.filteredEvents();
            checkSizeT("filteredEvents count", events.size(), 3);
        }

        // ── Test 4: filteredEvents() respects event filter mask ──────────
        std::cout << "\n[TEST] === filteredEvents() with mask ===\n";
        {
            SimulationRecorder::RunMetadata meta{};
            meta.runId = 3;
            meta.scenarioName = "test";
            meta.mapHash = "hash";
            meta.maxSteps = 10;
            meta.noiseLevel = 0.0;
            meta.seed = 99;
            meta.startTime = SimulationRecorder::timestampNow();
            meta.wallTimeMs = 0.0;
            meta.totalSeekers = 1;
            meta.totalTargets = 1;
            meta.totalDetectors = 1;
            meta.totalInterceptors = 1;
            meta.totalAttackers = 1;

            SimulationRecorder recorder(meta);

            recorder.recordEvent(0, 1, 0, 0);
            recorder.recordEvent(1, SimulationRecorder::EVENT_INTERCEPT, 0, 1);
            recorder.recordEvent(2, SimulationRecorder::EVENT_TARGET_DESTROYED, 1, 0);
            recorder.recordEvent(3, SimulationRecorder::EVENT_SEEKER_REACHED, 1, 1);

            recorder.setEventFilter(SimulationRecorder::EVENT_DETECTION);
            auto events1 = recorder.filteredEvents();
            checkSizeT("filter mask=DETECTION", events1.size(), 1);

            recorder.setEventFilter(SimulationRecorder::EVENT_INTERCEPT);
            auto events2 = recorder.filteredEvents();
            checkSizeT("filter mask=INTERCEPT", events2.size(), 1);

            recorder.setEventFilter(SimulationRecorder::EVENT_DETECTION | SimulationRecorder::EVENT_INTERCEPT);
            auto events3 = recorder.filteredEvents();
            checkSizeT("filter mask=DETECTION|INTERCEPT", events3.size(), 2);

            recorder.setEventFilter(0xF);
            auto eventsAll = recorder.filteredEvents();
            checkSizeT("filter mask=0xF (all)", eventsAll.size(), 4);
        }

        // ── Test 5: agentStats() returns correct statistics ───────────────
        std::cout << "\n[TEST] === agentStats() ===\n";
        {
            SimulationRecorder::RunMetadata meta{};
            meta.runId = 4;
            meta.scenarioName = "test";
            meta.mapHash = "hash";
            meta.maxSteps = 10;
            meta.noiseLevel = 0.0;
            meta.seed = 99;
            meta.startTime = SimulationRecorder::timestampNow();
            meta.wallTimeMs = 0.0;
            meta.totalSeekers = 1;
            meta.totalTargets = 1;
            meta.totalDetectors = 1;
            meta.totalInterceptors = 1;
            meta.totalAttackers = 1;

            SimulationRecorder recorder(meta);

            Simulation sim(fixture.map, fixture.config, 10, 99);
            sim.setRecorder(&recorder);

            for (int i = 0; i < 5; i++) {
                sim.stepOnce();
            }

            auto stats = recorder.agentStats();
            checkGreater("agentStats count", static_cast<int>(stats.size()), 0);

            bool hasSeeker = false;
            bool hasTarget = false;
            bool hasAttacker = false;
            for (const auto& s : stats) {
                if (std::get<1>(s) == "seeker") hasSeeker = true;
                if (std::get<1>(s) == "target") hasTarget = true;
                if (std::get<1>(s) == "attacker") hasAttacker = true;
            }
            checkBool("stats has seeker", hasSeeker, true);
            checkBool("stats has target", hasTarget, true);
            checkBool("stats has attacker", hasAttacker, true);
        }

        // ── Test 6: saveJSON() produces valid JSON ───────────────────────
        std::cout << "\n[TEST] === saveJSON() ===\n";
        {
            SimulationRecorder::RunMetadata meta{};
            meta.runId = 5;
            meta.scenarioName = "json_test";
            meta.mapHash = "hash123";
            meta.maxSteps = 10;
            meta.noiseLevel = 0.1;
            meta.seed = 55;
            meta.startTime = SimulationRecorder::timestampNow();
            meta.wallTimeMs = 5.0;
            meta.totalSeekers = 1;
            meta.totalTargets = 1;
            meta.totalDetectors = 1;
            meta.totalInterceptors = 1;
            meta.totalAttackers = 1;

            SimulationRecorder recorder(meta);

            Simulation sim(fixture.map, fixture.config, 10, 55);
            sim.setRecorder(&recorder);

            for (int i = 0; i < 3; i++) {
                sim.stepOnce();
            }

            std::string jsonPath = "tests/fixtures/test_recorder_output.json";
            bool saved = recorder.saveJSON(jsonPath);
            checkBool("saveJSON returns true", saved, true);

            std::ifstream jsonFile(jsonPath);
            checkBool("JSON file exists", jsonFile.is_open(), true);

            if (jsonFile.is_open()) {
                std::string content((std::istreambuf_iterator<char>(jsonFile)),
                                     std::istreambuf_iterator<char>());
                jsonFile.close();

                bool hasMetadata = content.find("\"metadata\"") != std::string::npos;
                bool hasSteps = content.find("\"steps\"") != std::string::npos;
                bool hasEventStream = content.find("\"eventStream\"") != std::string::npos;
                bool hasAgentStats = content.find("\"agentStats\"") != std::string::npos;
                bool hasRunId = content.find("\"runId\":5") != std::string::npos;
                bool hasScenario = content.find("json_test") != std::string::npos;

                checkBool("JSON has metadata", hasMetadata, true);
                checkBool("JSON has steps", hasSteps, true);
                checkBool("JSON has eventStream", hasEventStream, true);
                checkBool("JSON has agentStats", hasAgentStats, true);
                checkBool("JSON has correct runId", hasRunId, true);
                checkBool("JSON has scenario name", hasScenario, true);
            }
        }

        // ── Test 7: saveBinary() produces valid binary data ──────────────
        std::cout << "\n[TEST] === saveBinary() ===\n";
        {
            SimulationRecorder::RunMetadata meta{};
            meta.runId = 6;
            meta.scenarioName = "binary_test";
            meta.mapHash = "hash456";
            meta.maxSteps = 10;
            meta.noiseLevel = 0.2;
            meta.seed = 77;
            meta.startTime = SimulationRecorder::timestampNow();
            meta.wallTimeMs = 3.0;
            meta.totalSeekers = 1;
            meta.totalTargets = 1;
            meta.totalDetectors = 1;
            meta.totalInterceptors = 1;
            meta.totalAttackers = 1;

            SimulationRecorder recorder(meta);

            Simulation sim(fixture.map, fixture.config, 10, 77);
            sim.setRecorder(&recorder);

            for (int i = 0; i < 4; i++) {
                sim.stepOnce();
            }

            std::string binPath = "tests/fixtures/test_recorder_output.bin";
            bool saved = recorder.saveBinary(binPath);
            checkBool("saveBinary returns true", saved, true);

            std::ifstream binFile(binPath, std::ios::binary | std::ios::ate);
            checkBool("Binary file exists", binFile.is_open(), true);

            if (binFile.is_open()) {
                auto size = binFile.tellg();
                binFile.seekg(0, std::ios::beg);

                checkGreater("Binary file size", static_cast<int>(size), 0);

                char magic[4];
                binFile.read(magic, 4);
                bool magicValid = (magic[0] == 'U' && magic[1] == 'U' && magic[2] == 'V' && magic[3] == 'R');
                checkBool("Binary magic number", magicValid, true);

                binFile.close();
            }
        }

        // ── Test 8: clear() resets all data ──────────────────────────────
        std::cout << "\n[TEST] === clear() ===\n";
        {
            SimulationRecorder::RunMetadata meta{};
            meta.runId = 7;
            meta.scenarioName = "clear_test";
            meta.mapHash = "hash";
            meta.maxSteps = 10;
            meta.noiseLevel = 0.0;
            meta.seed = 99;
            meta.startTime = SimulationRecorder::timestampNow();
            meta.wallTimeMs = 0.0;
            meta.totalSeekers = 1;
            meta.totalTargets = 1;
            meta.totalDetectors = 1;
            meta.totalInterceptors = 1;
            meta.totalAttackers = 1;

            SimulationRecorder recorder(meta);

            Simulation sim(fixture.map, fixture.config, 10, 99);
            sim.setRecorder(&recorder);

            for (int i = 0; i < 3; i++) {
                if (sim.isFinished()) break;
                sim.stepOnce();
            }

            checkBool("hasData before clear", recorder.hasData(), true);
            checkGreater("stepCount before clear", static_cast<int>(recorder.stepCount()), 0);

            recorder.clear();

            checkBool("hasData after clear", recorder.hasData(), false);
            checkSizeT("stepCount after clear", recorder.stepCount(), 0);
        }

        // ── Test 9: stepCount() and hasData() work correctly ─────────────
        std::cout << "\n[TEST] === stepCount() and hasData() ===\n";
        {
            SimulationRecorder::RunMetadata meta{};
            meta.runId = 8;
            meta.scenarioName = "count_test";
            meta.mapHash = "hash";
            meta.maxSteps = 10;
            meta.noiseLevel = 0.0;
            meta.seed = 99;
            meta.startTime = SimulationRecorder::timestampNow();
            meta.wallTimeMs = 0.0;
            meta.totalSeekers = 1;
            meta.totalTargets = 1;
            meta.totalDetectors = 1;
            meta.totalInterceptors = 1;
            meta.totalAttackers = 1;

            SimulationRecorder recorder(meta);

            checkSizeT("stepCount at start", recorder.stepCount(), 0);
            checkBool("hasData at start", recorder.hasData(), false);

            Simulation sim(fixture.map, fixture.config, 100, 99);
            sim.setRecorder(&recorder);

            sim.stepOnce();
            checkSizeT("stepCount after 1 step", recorder.stepCount(), 1);
            checkBool("hasData after 1 step", recorder.hasData(), true);

            if (!sim.isFinished()) {
                sim.stepOnce();
                checkSizeT("stepCount after 2 steps", recorder.stepCount(), 2);

                if (!sim.isFinished()) {
                    sim.stepOnce();
                    checkSizeT("stepCount after 3 steps", recorder.stepCount(), 3);
                }
            }
        }

        // ── Test 10: Full simulation run with recorder attached ──────────
        std::cout << "\n[TEST] === Full simulation with recorder ===\n";
        {
            SimulationRecorder::RunMetadata meta{};
            meta.runId = 9;
            meta.scenarioName = "full_run";
            meta.mapHash = "fullhash";
            meta.maxSteps = 10;
            meta.noiseLevel = 0.0;
            meta.seed = 123;
            meta.startTime = SimulationRecorder::timestampNow();
            meta.wallTimeMs = 0.0;
            meta.totalSeekers = 1;
            meta.totalTargets = 1;
            meta.totalDetectors = 1;
            meta.totalInterceptors = 1;
            meta.totalAttackers = 1;

            SimulationRecorder recorder(meta);

            Simulation sim(fixture.map, fixture.config, 10, 123);
            sim.setRecorder(&recorder);

            int step = 0;
            while (!sim.isFinished() && step < 10) {
                sim.stepOnce();
                step++;
            }

            checkGreater("full run stepCount", static_cast<int>(recorder.stepCount()), 0);
            checkBool("full run hasData", recorder.hasData(), true);

            auto stats = recorder.agentStats();
            checkGreater("full run agentStats count", static_cast<int>(stats.size()), 0);

            std::string jsonPath = "tests/fixtures/test_full_run.json";
            bool saved = recorder.saveJSON(jsonPath);
            checkBool("full run saveJSON", saved, true);
        }

        // ── Verdict ──────────────────────────────────────────────────────
        std::cout << "\n[TEST] " << g_checks << " checks, " << g_failures << " failures\n";
        std::cout << "[TEST] " << (g_failures == 0 ? "ALL PASSED" : "SOME FAILED") << "\n";
        return (g_failures == 0) ? 0 : 1;

    } catch (const std::exception& e) {
        std::cerr << "[TEST] ERROR: " << e.what() << "\n";
        return 2;
    }
}
