/**
 * test_simulation.cpp
 *
 * Loads a scenario, runs the simulation, compares against stored results.
 *
 * Required files in tests/fixtures/:
 *   1. grid_cache.txt  — the map grid
 *   2. scenario.json   — map info + unit positions
 *   3. results.json    — expected simulation output
 *
 * Exit codes:
 *   0 = PASS
 *   1 = FAIL
 *   2 = ERROR
 *
 * Usage:
 *   ./test_sim                              (uses default paths)
 *   ./test_sim grid.txt scene.json res.json (override paths)
 */

#include "mapCreation.h"
#include "spawnConfig.h"
#include "simulation.h"
#include "simResult.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cmath>
#include <vector>
#include <algorithm>
#include <set>

// ════════════════════════════════════════════════════════════════════════════════
//  BASELINE LOADER
// ════════════════════════════════════════════════════════════════════════════════

struct BaselineData {
    int totalSteps;
    int targetsDestroyed;
    int totalTargets;
    int seekersThatReached;
    int totalSeekers;
    double avgStepsToTarget;
    bool allTargetsDestroyed;

    struct SeekerBaseline {
        int id;
        int stepsTaken;
        bool reachedTarget;
        int targetId;
    };
    std::vector<SeekerBaseline> seekers;

    struct TargetBaseline {
        int id;
        bool destroyed;
        int destroyedAtStep;
        int destroyedBySeeker;
    };
    std::vector<TargetBaseline> targets;
};

static double extractNum(const std::string& c, const std::string& key, size_t from = 0) {
    size_t pos = c.find("\"" + key + "\"", from);
    if (pos == std::string::npos) return -9999;
    size_t colon = c.find(":", pos);
    size_t start = colon + 1;
    while (start < c.size() && (c[start] == ' ' || c[start] == '\t')) start++;
    return std::stod(c.substr(start));
}

static bool extractBool(const std::string& c, const std::string& key, size_t from = 0) {
    size_t pos = c.find("\"" + key + "\"", from);
    if (pos == std::string::npos) return false;
    size_t colon = c.find(":", pos);
    size_t start = colon + 1;
    while (start < c.size() && c[start] == ' ') start++;
    return c.substr(start, 4) == "true";
}

BaselineData loadBaseline(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open baseline: " + filepath);
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    BaselineData b;
    size_t summaryPos = content.find("\"summary\"");
    b.totalSteps          = static_cast<int>(extractNum(content, "total_steps", summaryPos));
    b.targetsDestroyed    = static_cast<int>(extractNum(content, "targets_destroyed", summaryPos));
    b.totalTargets        = static_cast<int>(extractNum(content, "total_targets", summaryPos));
    b.seekersThatReached  = static_cast<int>(extractNum(content, "seekers_that_reached", summaryPos));
    b.totalSeekers        = static_cast<int>(extractNum(content, "total_seekers", summaryPos));
    b.avgStepsToTarget    = extractNum(content, "avg_steps_to_target", summaryPos);
    b.allTargetsDestroyed = extractBool(content, "all_targets_destroyed", summaryPos);

    // Parse per-seeker
    size_t seekersPos = content.find("\"seekers\"");
    if (seekersPos != std::string::npos) {
        size_t targetsSection = content.find("\"targets\"", seekersPos);
        size_t pos = seekersPos;
        while (true) {
            pos = content.find("\"id\"", pos);
            if (pos == std::string::npos || (targetsSection != std::string::npos && pos > targetsSection))
                break;

            BaselineData::SeekerBaseline sb;
            sb.id            = static_cast<int>(extractNum(content, "id", pos));
            sb.stepsTaken    = static_cast<int>(extractNum(content, "steps_taken", pos));
            sb.reachedTarget = extractBool(content, "reached_target", pos);
            sb.targetId      = static_cast<int>(extractNum(content, "target_id", pos));
            b.seekers.push_back(sb);

            pos = content.find("}", pos) + 1;
        }
    }

    // Parse per-target
    size_t targetsPos = content.find("\"targets\"");
    if (targetsPos != std::string::npos) {
        size_t pos = targetsPos;
        while (true) {
            pos = content.find("\"id\"", pos);
            if (pos == std::string::npos) break;

            BaselineData::TargetBaseline tb;
            tb.id                = static_cast<int>(extractNum(content, "id", pos));
            tb.destroyed         = extractBool(content, "destroyed", pos);
            tb.destroyedAtStep   = static_cast<int>(extractNum(content, "destroyed_at_step", pos));
            tb.destroyedBySeeker = static_cast<int>(extractNum(content, "destroyed_by_seeker", pos));
            b.targets.push_back(tb);

            pos = content.find("}", pos) + 1;
        }
    }

    return b;
}

// ════════════════════════════════════════════════════════════════════════════════
//  TEST RUNNER
// ════════════════════════════════════════════════════════════════════════════════

int main(int argc, char* argv[]) {
    // Default paths — no arguments required
    std::string gridPath     = "tests/fixtures/grid_cache.txt";
    std::string scenarioPath = "tests/fixtures/scenario.json";
    std::string baselinePath = "tests/fixtures/results.json";

    // Allow override via command line args
    if (argc >= 4) {
        gridPath     = argv[1];
        scenarioPath = argv[2];
        baselinePath = argv[3];
    }

    int failures = 0;
    int checks = 0;

    auto check = [&](const std::string& name, int actual, int expected) {
        checks++;
        bool pass = (actual == expected);
        std::cout << "  " << (pass ? "PASS" : "FAIL") << "  "
                  << name << ": got " << actual << ", expected " << expected << "\n";
        if (!pass) failures++;
    };

    auto checkBool = [&](const std::string& name, bool actual, bool expected) {
        checks++;
        bool pass = (actual == expected);
        std::cout << "  " << (pass ? "PASS" : "FAIL") << "  "
                  << name << ": got " << (actual ? "true" : "false")
                  << ", expected " << (expected ? "true" : "false") << "\n";
        if (!pass) failures++;
    };

    auto checkDouble = [&](const std::string& name, double actual, double expected, double tol = 0.01) {
        checks++;
        bool pass = std::abs(actual - expected) <= tol;
        std::cout << "  " << (pass ? "PASS" : "FAIL") << "  "
                  << name << ": got " << actual << ", expected " << expected << "\n";
        if (!pass) failures++;
    };

    auto checkInt = [&](const std::string& name, int actual, int expected) {
        checks++;
        bool pass = (actual == expected);
        std::cout << "  " << (pass ? "PASS" : "FAIL") << "  "
                  << name << ": got " << actual << ", expected " << expected << "\n";
        if (!pass) failures++;
    };

    try {
        // ── Load ─────────────────────────────────────────────────────
        std::cout << "[TEST] Loading grid:     " << gridPath << "\n";
        MapCreation map = MapCreation::fromCache(gridPath);

        std::cout << "[TEST] Loading scenario: " << scenarioPath << "\n";
        SpawnConfig config = SpawnConfig::loadJSON(scenarioPath);

        std::cout << "[TEST] Loading baseline: " << baselinePath << "\n";
        BaselineData baseline = loadBaseline(baselinePath);

        std::vector<std::vector<int>> cleanGrid = config.getGrid();
        for (auto& row : cleanGrid) {
            for (auto& cell : row) {
                if (cell == 2 || cell == 3 || cell == 4 || cell == 5 || cell == 7)
                    cell = 0;
            }
        }
        const MapInfo& info = config.getMapInfo();

        // ── Stamp units ──────────────────────────────────────────────
        for (const auto& unit : config.getUnits()) {
            int t = MapCreation::WATER;
            if (unit.type == "seeker")  t = MapCreation::SEEKER;
            if (unit.type == "target")  t = MapCreation::TARGET;
            map.placeUnit(unit.row, unit.col, t);
        }

        // ── Run ──────────────────────────────────────────────────────
        std::cout << "[TEST] Running simulation...\n\n";
        Simulation sim(map, config, 2000);
        SimResult result = sim.run();

        // ── Compare summary ──────────────────────────────────────────
        std::cout << "\n[TEST] === Summary ===\n";
        check("total_steps",          result.totalSteps,         baseline.totalSteps);
        check("targets_destroyed",    result.targetsDestroyed,   baseline.targetsDestroyed);
        check("seekers_that_reached", result.seekersThatReached, baseline.seekersThatReached);
        checkBool("all_targets_destroyed", result.allTargetsDestroyed, baseline.allTargetsDestroyed);
        checkDouble("avg_steps_to_target", result.avgStepsToTarget, baseline.avgStepsToTarget);

        // ── Compare per-seeker ───────────────────────────────────────
        std::cout << "\n[TEST] === Seekers ===\n";
        for (int i = 0; i < (int)baseline.seekers.size() && i < (int)result.seekerResults.size(); i++) {
            const auto& a = result.seekerResults[i];
            const auto& e = baseline.seekers[i];
            std::string p = "seeker_" + std::to_string(a.id);
            check(p + ".steps_taken", a.stepsTaken, e.stepsTaken);
            checkBool(p + ".reached",  a.reachedTarget, e.reachedTarget);
            check(p + ".target_id",   a.targetId, e.targetId);
        }

        // ── Compare per-target ───────────────────────────────────────
        std::cout << "\n[TEST] === Targets ===\n";
        for (int i = 0; i < (int)baseline.targets.size() && i < (int)result.targetResults.size(); i++) {
            const auto& a = result.targetResults[i];
            const auto& e = baseline.targets[i];
            std::string p = "target_" + std::to_string(a.id);
            checkBool(p + ".destroyed",        a.destroyed, e.destroyed);
            check(p + ".destroyed_at_step",    a.destroyedAtStep, e.destroyedAtStep);
            check(p + ".destroyed_by_seeker",  a.destroyedBySeeker, e.destroyedBySeeker);
        }

        // ── Edge case: zero detectors, interceptors present ──────────
        std::cout << "\n[TEST] === Edge Cases ===\n";
        map.clearAllUnits();
        SpawnConfig edge1;
        edge1.addUnit("seeker", 32, 76);
        edge1.addUnit("target", 13, 15);
        edge1.addUnit("interceptor", 14, 15);
        edge1.setInterceptorRadius(3.0);
        for (const auto& unit : edge1.getUnits()) {
            int t = MapCreation::WATER;
            if (unit.type == "seeker")      t = MapCreation::SEEKER;
            else if (unit.type == "target") t = MapCreation::TARGET;
            else if (unit.type == "interceptor") t = MapCreation::INTERCEPTOR;
            if (t != MapCreation::WATER) map.placeUnit(unit.row, unit.col, t);
        }
        {
            Simulation edgeSim(map, edge1, 2000, 99);
            SimResult edgeResult = edgeSim.run();
            check("zero_detectors_total_steps", edgeResult.totalSteps > 0, true);
            check("zero_detectors_seekers_reached", edgeResult.seekersThatReached, 1);
        }

        // ── Edge case: zero interceptors, detectors present ──────────
        map.clearAllUnits();
        SpawnConfig edge2;
        edge2.addUnit("seeker", 32, 76);
        edge2.addUnit("target", 13, 15);
        edge2.addUnit("detector", 14, 15);
        edge2.setDetectorRadius(4.0);
        for (const auto& unit : edge2.getUnits()) {
            int t = MapCreation::WATER;
            if (unit.type == "seeker")      t = MapCreation::SEEKER;
            else if (unit.type == "target") t = MapCreation::TARGET;
            else if (unit.type == "detector")   t = MapCreation::DETECTOR;
            if (t != MapCreation::WATER) map.placeUnit(unit.row, unit.col, t);
        }
        {
            Simulation edgeSim(map, edge2, 2000, 99);
            SimResult edgeResult = edgeSim.run();
            check("zero_interceptors_total_steps", edgeResult.totalSteps > 0, true);
            check("zero_interceptors_seekers_reached", edgeResult.seekersThatReached, 1);
        }

        // ── Edge case: critical target tracked in CSV output ─────────
        {
            SpawnConfig critConfig;
            critConfig.setMapData(info, cleanGrid);
            critConfig.addUnit("seeker", 32, 76);
            critConfig.addUnit("target", 13, 15, "", true);
            critConfig.setDetectorRadius(4.0);
            critConfig.setInterceptorRadius(3.0);

            MapCreation critMap = MapCreation::fromGridData(
                cleanGrid, info.cellsN, info.canvasWidth, info.canvasHeight);
            for (const auto& unit : critConfig.getUnits()) {
                int t = MapCreation::WATER;
                if (unit.type == "seeker")      t = MapCreation::SEEKER;
                else if (unit.type == "target") t = MapCreation::TARGET;
                if (t != MapCreation::WATER) critMap.placeUnit(unit.row, unit.col, t);
            }

            Simulation critSim(critMap, critConfig, 2000, 99);
            SimResult critResult = critSim.run();
            critResult.computeSummary();

            std::string csvPath = "tests/fixtures/critical_test.csv";
            std::ofstream csvOut(csvPath, std::ios::trunc);
            if (csvOut.is_open()) csvOut.close();

            critResult.saveCSV(csvPath, 0);

            std::ifstream csvFile(csvPath);
            std::string csvLine;
            bool foundCritical = false;
            bool criticalVal = false;
            if (csvFile.is_open()) {
                std::getline(csvFile, csvLine);
                std::getline(csvFile, csvLine);
                size_t pos = 0;
                int fieldIndex = 0;
                while ((pos = csvLine.find(',', pos)) != std::string::npos) {
                    fieldIndex++;
                    if (fieldIndex == 6) {
                        std::string val = csvLine.substr(pos + 1);
                        size_t nextComma = val.find(',');
                        if (nextComma != std::string::npos) val = val.substr(0, nextComma);
                        criticalVal = (val == "true" || val == "1");
                        foundCritical = true;
                        break;
                    }
                    pos++;
                }
                csvFile.close();
            }
            checkBool("critical_asset_reached_in_csv", foundCritical && criticalVal, true);
        }

        // ── Edge case: GA batch mode (--repeat) produces valid CSV ─────
        {
            SpawnConfig gaConfig;
            gaConfig.setMapData(config.getMapInfo(), cleanGrid);
            gaConfig.addUnit("seeker", 32, 76);
            gaConfig.addUnit("target", 13, 15);
            gaConfig.setDetectorRadius(4.0);
            gaConfig.setInterceptorRadius(3.0);

            std::string gaCsv = "tests/fixtures/ga_batch_test.csv";
            {
                std::ofstream out(gaCsv, std::ios::trunc);
                if (out.is_open()) out.close();
            }

            MapCreation gaMap = MapCreation::fromGridData(
                cleanGrid, info.cellsN, info.canvasWidth, info.canvasHeight);
            for (const auto& unit : gaConfig.getUnits()) {
                int t = MapCreation::WATER;
                if (unit.type == "seeker")      t = MapCreation::SEEKER;
                else if (unit.type == "target") t = MapCreation::TARGET;
                if (t != MapCreation::WATER) gaMap.placeUnit(unit.row, unit.col, t);
            }

            Simulation gaSim(gaMap, gaConfig, 2000, 99);
            SimResult gaResult = gaSim.run();
            gaResult.computeSummary();
            gaResult.saveCSV(gaCsv, 0);

            std::ifstream csvFile(gaCsv);
            std::string header;
            if (csvFile.is_open()) {
                std::getline(csvFile, header);
                csvFile.close();
            }
            bool hasHeader = header.find("run_id") != std::string::npos &&
                             header.find("blue_cost") != std::string::npos &&
                             header.find("red_cost") != std::string::npos &&
                             header.find("loss_exchange_ratio") != std::string::npos &&
                             header.find("targets_destroyed") != std::string::npos &&
                             header.find("total_targets") != std::string::npos &&
                             header.find("critical_asset_reached") != std::string::npos &&
                             header.find("total_steps") != std::string::npos &&
                             header.find("mission_success_rate") != std::string::npos &&
                             header.find("interceptor_engagements") != std::string::npos;
            checkBool("ga_batch_csv_header_valid", hasHeader, true);
        }

        // ── Edge case: Simulation::runBatch() direct C++ API ───────────
        {
            std::vector<SpawnConfig> batchConfigs;
            for (int i = 0; i < 3; i++) {
                SpawnConfig sc;
                sc.setMapData(config.getMapInfo(), cleanGrid);
                sc.addUnit("seeker", 32, 76);
                sc.addUnit("target", 13, 15);
                sc.setDetectorRadius(4.0);
                sc.setInterceptorRadius(3.0);
                batchConfigs.push_back(sc);
            }

            auto batchResults = Simulation::runBatch(batchConfigs, 2000, 99);
            checkInt("runBatch_count", static_cast<int>(batchResults.size()), 3);

            bool allHaveSummary = true;
            bool allHaveSeekers = true;
            bool allHaveTargets = true;
            for (const auto& r : batchResults) {
                if (r.totalSteps <= 0) allHaveSummary = false;
                if (r.seekerResults.empty()) allHaveSeekers = false;
                if (r.targetResults.empty()) allHaveTargets = false;
            }
            checkBool("runBatch_all_have_summary", allHaveSummary, true);
            checkBool("runBatch_all_have_seekers", allHaveSeekers, true);
            checkBool("runBatch_all_have_targets", allHaveTargets, true);
        }

        // ── Verdict ──────────────────────────────────────────────────
        std::cout << "\n[TEST] " << checks << " checks, " << failures << " failures\n";
        std::cout << "[TEST] " << (failures == 0 ? "ALL PASSED" : "SOME FAILED") << "\n";
        return (failures == 0) ? 0 : 1;

    } catch (const std::exception& e) {
        std::cerr << "[TEST] ERROR: " << e.what() << "\n";
        return 2;
    }
}