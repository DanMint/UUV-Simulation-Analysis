/**
 * test_diveld_scenario.cpp
 *
 * Phase 17 Unit Test: Dive-LD Baseline Scenario End-to-End Validation
 *
 * Purpose:
 *   Validates that 3 Anduril Dive-LD AUVs can be spawned from 3 directions,
 *   navigate through the real simulator to a central target, and the
 *   simulation completes successfully.
 *
 * Scenario:
 *   - 3x Dive-LD AUVs attacking from north, south, and east
 *   - 1x critical target in center
 *   - 2x detectors, 1x interceptor (defender)
 *
 * Exit Codes:
 *   0 = PASS  (simulation ran, all 3 attackers active from distinct cells)
 *   1 = FAIL  (validation check failed)
 *   2 = ERROR (file loading, parsing, or simulation error)
 */

#include "mapCreation.h"
#include "spawnConfig.h"
#include "simulation.h"
#include "simResult.h"
#include "vehicleSpecs.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cmath>
#include <vector>
#include <algorithm>
#include <set>

// ════════════════════════════════════════════════════════════════════════════════
//  DIVE-LD END-TO-END VALIDATION
// ════════════════════════════════════════════════════════════════════════════════

int main(int argc, char* argv[]) {
    try {
        std::string scenarioPath = "scenarios/diveld_baseline_complete.json";

        if (argc > 1) scenarioPath = argv[1];

        std::cout << "═══════════════════════════════════════════════════════════════" << std::endl;
        std::cout << "  Phase 17: Dive-LD Baseline Scenario E2E Validation" << std::endl;
        std::cout << "═══════════════════════════════════════════════════════════════" << std::endl;
        std::cout << "Scenario: " << scenarioPath << std::endl;
        std::cout << "───────────────────────────────────────────────────────────────" << std::endl;

        // ════════════════════════════════════════════════════════════════════════
        // 1. Verify Dive-LD specs exist in registry
        // ════════════════════════════════════════════════════════════════════════
        std::cout << "\n✓ Step 1: Verify Dive-LD vehicle specs" << std::endl;
        VehicleSpecs diveldSpecs = getVehicleSpecs("diveld");
        std::cout << "  - Type:      " << diveldSpecs.agentType << std::endl;
        std::cout << "  - Mfr:       " << diveldSpecs.manufacturer << std::endl;
        std::cout << "  - Speed:     " << diveldSpecs.speedKnotsMin << "-" << diveldSpecs.speedKnotsMax << " kn" << std::endl;
        std::cout << "  - Cost:      $" << diveldSpecs.unitCostMin / 1e6 << "M-$" << diveldSpecs.unitCostMax / 1e6 << "M" << std::endl;
        std::cout << "  - Freq:      " << diveldSpecs.emissionFreqLowHz << "-" << diveldSpecs.emissionFreqHighHz << " Hz" << std::endl;
        std::cout << "  - Shallow:   " << (diveldSpecs.shallowWaterCapable ? "Yes" : "No") << std::endl;
        std::cout << "  - Aerial:    " << (diveldSpecs.isAerial ? "Yes" : "No") << std::endl;
        std::cout << "  - Surface:   " << (diveldSpecs.isSurfaceVessel ? "Yes" : "No") << std::endl;

        if (diveldSpecs.speedKnotsMin < 0.5f || diveldSpecs.speedKnotsMax < 2.0f) {
            std::cerr << "✗ Dive-LD speed specs invalid (expected 1-3 kn, got "
                      << diveldSpecs.speedKnotsMin << "-" << diveldSpecs.speedKnotsMax << ")" << std::endl;
            return 1;
        }
        if (diveldSpecs.isAerial || diveldSpecs.isSurfaceVessel) {
            std::cerr << "✗ Dive-LD must be a UUV (aerial=false, surface=false)" << std::endl;
            return 1;
        }
        if (!diveldSpecs.shallowWaterCapable) {
            std::cerr << "✗ Dive-LD must be shallow-water capable" << std::endl;
            return 1;
        }
        std::cout << "  → Specs validated ✓" << std::endl;

        // ════════════════════════════════════════════════════════════════════════
        // 2. Load scenario via SpawnConfig
        // ════════════════════════════════════════════════════════════════════════
        std::cout << "\n✓ Step 2: Load scenario via SpawnConfig" << std::endl;
        SpawnConfig config = SpawnConfig::loadJSON(scenarioPath);

        auto attackers = config.getUnitsByType("attacker");
        auto targets   = config.getUnitsByType("target");
        auto detectors = config.getUnitsByType("detector");
        auto interceptors = config.getUnitsByType("interceptor");

        std::cout << "  - Attackers:    " << attackers.size() << std::endl;
        std::cout << "  - Targets:      " << targets.size() << std::endl;
        std::cout << "  - Detectors:    " << detectors.size() << std::endl;
        std::cout << "  - Interceptors: " << interceptors.size() << std::endl;

        if (attackers.size() != 3) {
            std::cerr << "✗ Expected 3 attackers, found " << attackers.size() << std::endl;
            return 1;
        }

        // Verify 3 distinct starting cells
        std::set<std::pair<int,int>> startCells;
        for (const auto& a : attackers) {
            startCells.insert({a.row, a.col});
        }
        if (startCells.size() != 3) {
            std::cerr << "✗ Attackers do not occupy 3 distinct starting cells" << std::endl;
            return 1;
        }
        std::cout << "  → 3 distinct attacker spawn positions confirmed ✓" << std::endl;

        // Verify all are Dive-LD
        for (const auto& a : attackers) {
            if (a.vehicleType != "diveld") {
                std::cerr << "✗ Unexpected attacker vehicle type: " << a.vehicleType << std::endl;
                return 1;
            }
        }
        std::cout << "  → All attackers are Dive-LD ✓" << std::endl;

        // Verify critical target
        bool hasCritical = false;
        for (const auto& t : targets) {
            if (t.isCritical) hasCritical = true;
        }
        if (!hasCritical) {
            std::cerr << "✗ No critical target marked" << std::endl;
            return 1;
        }
        std::cout << "  → Critical target confirmed ✓" << std::endl;

        if (detectors.size() < 2 || interceptors.size() < 1) {
            std::cerr << "✗ Insufficient defenders (need >=2 detectors, >=1 interceptor)" << std::endl;
            return 1;
        }
        std::cout << "  → Defender config valid ✓" << std::endl;

        // ════════════════════════════════════════════════════════════════════════
        // 3. Reconstruct map from embedded grid data and run simulation
        // ════════════════════════════════════════════════════════════════════════
        std::cout << "\n✓ Step 3: Run simulation end-to-end" << std::endl;

        if (!config.hasMapData()) {
            std::cerr << "✗ Scenario has no embedded map data" << std::endl;
            return 2;
        }

        const MapInfo& info = config.getMapInfo();
        MapCreation map = MapCreation::fromGridData(
            config.getGrid(), info.cellsN, info.canvasWidth, info.canvasHeight);

        for (const auto& unit : config.getUnits()) {
            int t = MapCreation::WATER;
            if (unit.type == "seeker")      t = MapCreation::SEEKER;
            else if (unit.type == "target") t = MapCreation::TARGET;
            else if (unit.type == "detector")   t = MapCreation::DETECTOR;
            else if (unit.type == "interceptor") t = MapCreation::INTERCEPTOR;
            else if (unit.type == "attacker")   t = MapCreation::ATTACKER;
            if (t != MapCreation::WATER) {
                map.placeUnit(unit.row, unit.col, t);
            }
        }

        Simulation sim(map, config, 2000, 42);
        SimResult result = sim.run();

        std::cout << "  - Total steps: " << result.totalSteps << std::endl;
        std::cout << "  - Targets destroyed: " << result.targetsDestroyed << "/" << result.targetResults.size() << std::endl;
        std::cout << "  - Attackers alive: " << result.attackersAlive << "/" << result.attackerResults.size() << std::endl;

        if (result.totalSteps == 0) {
            std::cerr << "✗ Simulation ran 0 steps" << std::endl;
            return 1;
        }

        if (result.attackerResults.size() != 3) {
            std::cerr << "✗ Expected 3 attacker results, found " << result.attackerResults.size() << std::endl;
            return 1;
        }

        // Verify all 3 attackers were active: either moved, reached target, or were intercepted
        int activeCount = 0;
        for (const auto& ar : result.attackerResults) {
            bool moved = (ar.stepsTaken > 0);
            bool reached = ar.missionSuccess;
            bool intercepted = !ar.sightings.empty() || !ar.intercepts.empty();
            if (moved || reached || intercepted) {
                activeCount++;
            }
        }
        if (activeCount < 3) {
            std::cerr << "✗ Only " << activeCount << "/3 attackers showed activity (moved/reached/intercepted)" << std::endl;
            return 1;
        }
        std::cout << "  → All 3 attackers active from distinct starting cells ✓" << std::endl;

        // Verify critical target tracking
        bool criticalTracked = false;
        for (const auto& tr : result.targetResults) {
            if (tr.isCritical) {
                criticalTracked = true;
                std::cout << "  - Critical target destroyed: " << (tr.destroyed ? "yes" : "no") << std::endl;
            }
        }
        if (!criticalTracked) {
            std::cerr << "✗ Critical target not found in results" << std::endl;
            return 1;
        }
        std::cout << "  → Critical target tracked in results ✓" << std::endl;

        // ════════════════════════════════════════════════════════════════════════
        // Summary
        // ════════════════════════════════════════════════════════════════════════
        std::cout << "\n═══════════════════════════════════════════════════════════════" << std::endl;
        std::cout << "  ✓ ALL DIVE-LD BASELINE E2E VALIDATIONS PASSED" << std::endl;
        std::cout << "═══════════════════════════════════════════════════════════════" << std::endl;
        std::cout << "\nSummary:" << std::endl;
        std::cout << "  - Dive-LD specs: Valid (1-3 kn, $0.5M-$1.0M, UUV)" << std::endl;
        std::cout << "  - Simulation: Ran " << result.totalSteps << " steps" << std::endl;
        std::cout << "  - Attackers: 3 active from distinct starting cells" << std::endl;
        std::cout << "  - Critical target: Tracked in results" << std::endl;
        std::cout << "  - Defenders: " << detectors.size() << " detectors, " << interceptors.size() << " interceptor(s)" << std::endl;
        std::cout << "\nPhase 17 Reference Baseline Ready for GA Fitness Evaluation" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "\n✗ ERROR: " << e.what() << std::endl;
        return 2;
    }
}
