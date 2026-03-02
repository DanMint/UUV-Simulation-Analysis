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

    try {
        // ── Load ─────────────────────────────────────────────────────
        std::cout << "[TEST] Loading grid:     " << gridPath << "\n";
        MapCreation map = MapCreation::fromCache(gridPath);

        std::cout << "[TEST] Loading scenario: " << scenarioPath << "\n";
        SpawnConfig config = SpawnConfig::loadJSON(scenarioPath);

        std::cout << "[TEST] Loading baseline: " << baselinePath << "\n";
        BaselineData baseline = loadBaseline(baselinePath);

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

        // ── Verdict ──────────────────────────────────────────────────
        std::cout << "\n[TEST] " << checks << " checks, " << failures << " failures\n";
        std::cout << "[TEST] " << (failures == 0 ? "ALL PASSED" : "SOME FAILED") << "\n";
        return (failures == 0) ? 0 : 1;

    } catch (const std::exception& e) {
        std::cerr << "[TEST] ERROR: " << e.what() << "\n";
        return 2;
    }
}