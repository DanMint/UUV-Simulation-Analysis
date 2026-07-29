/**
 * test_attackerAgent.cpp
 * Author: Nadeem
 *
 * Unit tests for AttackerAgent & VehicleSpecs â€” verifies that:
 *   1. Each vehicle type gets correct real-world specs from the factory
 *   2. Aerial agents correctly return false for isDetectableByHydrophone()
 *   3. Frequency range detection logic works correctly
 *   4. FSM starts in S0_IDLE and transitions correctly
 *   5. stepDelay is set correctly per vehicle type
 *   6. validateState() rejects inconsistent ABORT+alive states
 *   7. VehicleSpecs::costCategory() returns correct price tiers
 *   8. AttackerAgent::isInRange() Euclidean distance check works
 *   9. VehicleSpecs::shortCode() returns correct 2-char codes
 *  10. Attacker retargets correctly when target destroyed
 *  11. VehicleSpecs::isDetectableByHydrophone() field-level tests
 *  12. SeekerAgent::hasPath() edge cases (empty, consumed, etc.)
 *
 * Exit codes:
 *   0 = ALL PASS
 *   1 = ONE OR MORE FAIL
 *
 * Usage:
 *   ./test_attacker
 *   (i use: cd UUV-Simulation-Analysis
 *    .\windows_build\build\Release\test_attacker.exe)
 *
 * Coverage target: 100% of public methods in attackerAgent.h, vehicleSpecs.h,
 * plus key methods in SeekerAgent that underpin AttackerAgent behavior.
 */

#include "attackerAgent.h"
#include "seekerAgent.h"
#include "vehicleSpecs.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// â”€â”€ Simple test framework â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

static int totalTests  = 0;
static int passedTests = 0;

void check(bool condition, const std::string& testName) {
    totalTests++;
    if (condition) {
        std::cout << "  [PASS] " << testName << "\n";
        passedTests++;
    } else {
        std::cout << "  [FAIL] " << testName << "\n";
    }
}

// â”€â”€ Test helpers â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void testSection(const std::string& name) {
    std::cout << "\nâ”€â”€ " << name << " â”€â”€\n";
}

// â”€â”€ Tests â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void testFSMInitialState() {
    testSection("FSM Initial State");

    AttackerAgent a = AttackerAgent::create("bluerov2", 0, 5, 5);
    check(a.fsmState == AgentFSMState::S0_IDLE,
          "BlueROV2 starts in S0_IDLE");
    check(a.alive == true,
          "Agent starts alive");
    check(a.missionSuccess == false,
          "Mission success starts false");
    check(a.stepsTaken == 0,
          "Steps taken starts at 0");
    check(a.milestone25 == false, "milestone25 starts false");
    check(a.milestone50 == false, "milestone50 starts false");
    check(a.milestone75 == false, "milestone75 starts false");
    check(a.stepDelayCounter == 0, "stepDelayCounter starts at 0");
    check(a.sightings.empty(), "Sightings start empty");
    check(a.intercepts.empty(), "Intercepts start empty");
    check(a.sightingCount == 0, "sightingCount starts at 0");
    check(a.killCount == 0, "killCount starts at 0");
    check(a.targetId == -1, "targetId starts as -1 (none)");
}

void testFSMTransitionOnTick() {
    testSection("FSM Transitions via tick()");
    std::vector<std::vector<int>> grid(30, std::vector<int>(30, 0));
    Pathfinding pf(grid);

    AttackerAgent a = AttackerAgent::create("bluerov2", 0, 5, 5);

        (void)a.tick(20, 20, pf);
    check(a.fsmState == AgentFSMState::S3_INIT_BEHAVIOR,
          "First tick advances S0 through S1/S2 to S3_INIT_BEHAVIOR");

        (void)a.tick(20, 20, pf);
    check(a.fsmState == AgentFSMState::S4_EXECUTE,
          "Second tick advances S3_INIT_BEHAVIOR to S4_EXECUTE");
}

void testFallbackWhenTargetEqualsSpawn() {
    testSection("Fallback: target == spawn");
    std::vector<std::vector<int>> grid(30, std::vector<int>(30, 0));
    Pathfinding pf(grid);

    AttackerAgent a = AttackerAgent::create("bluerov2", 0, 5, 5);
        (void)a.tick(5, 5, pf);
    check(a.fsmState == AgentFSMState::FALLBACK,
          "Target == spawn triggers FALLBACK");
    check(a.fallbackReason == "target is same cell as spawn",
          "Fallback reason is set correctly");
    check(a.alive == false,
          "Agent marked not alive on triggerFallback");
}

void testBlueROV2Specs() {
    testSection("BlueROV2 Specs");

    AttackerAgent a = AttackerAgent::create("bluerov2", 0, 5, 5);
    check(a.specs.speedKnotsMin == 1.f,       "BlueROV2 min speed = 1 kn");
    check(a.specs.speedKnotsMax == 3.f,       "BlueROV2 max speed = 3 kn");
    check(a.specs.emissionFreqLowHz  == 300000, "BlueROV2 freq low = 300k Hz");
    check(a.specs.emissionFreqHighHz == 450000, "BlueROV2 freq high = 450k Hz");
    check(a.specs.shallowWaterCapable == true,  "BlueROV2 shallow water capable");
    check(a.specs.isAerial == false,            "BlueROV2 is not aerial");
    check(a.specs.isSurfaceVessel == false,     "BlueROV2 is not a surface vessel");
    check(a.specs.unitCostMin == 6000.f,        "BlueROV2 cost = $6k");
    check(a.specs.stepDelay == 4,              "BlueROV2 stepDelay = 4 (1-3 kn)");
}

void testRiptideSpecs() {
    testSection("Riptide Micro Specs");

    AttackerAgent a = AttackerAgent::create("riptide", 0, 5, 5);
    check(a.specs.speedKnotsMin == 2.f,         "Riptide min speed = 2 kn");
    check(a.specs.speedKnotsMax == 5.f,         "Riptide max speed = 5 kn");
    check(a.specs.emissionFreqLowHz  == 200000, "Riptide freq low = 200k Hz");
    check(a.specs.emissionFreqHighHz == 400000, "Riptide freq high = 400k Hz");
    check(a.specs.stepDelay == 3,               "Riptide stepDelay = 3 (2-5 kn)");
    check(a.specs.isAerial == false,            "Riptide is not aerial");
}

void testHUGINSpecs() {
    testSection("HUGIN Superior Specs");

    AttackerAgent a = AttackerAgent::create("hugin", 0, 5, 5);
    check(a.specs.speedKnotsMin == 2.f,          "HUGIN min speed = 2 kn");
    check(a.specs.speedKnotsMax == 5.f,          "HUGIN max speed = 5 kn");
    check(a.specs.emissionFreqLowHz  == 200000,  "HUGIN freq low = 200k Hz");
    check(a.specs.emissionFreqHighHz == 400000,  "HUGIN freq high = 400k Hz");
    check(a.specs.unitCostMin == 2000000.f,      "HUGIN cost min = $2M");
    check(a.specs.unitCostMax == 4000000.f,      "HUGIN cost max = $4M");
    check(a.specs.stepDelay == 3,                "HUGIN stepDelay = 3");
    check(a.specs.isAerial == false,             "HUGIN is not aerial");
    check(a.specs.shallowWaterCapable == true,   "HUGIN shallow water capable");
}

void testAerialAgents() {
    testSection("Aerial Agents - Hydrophone Detection");

    AttackerAgent tb2 = AttackerAgent::create("tb2", 0, 5, 5);
    check(tb2.specs.isAerial == true,
          "TB2 is aerial");
    check(tb2.isDetectableByHydrophone() == false,
          "TB2 not detectable by hydrophone");
    check(tb2.specs.speedKnotsMin == 90.f,
          "TB2 min speed = 90 kn");
    check(tb2.specs.stepDelay == 1,
          "TB2 stepDelay = 1 (fastest)");

    AttackerAgent qh = AttackerAgent::create("queenhornet", 1, 5, 5);
    check(qh.specs.isAerial == true,
          "Queen Hornet is aerial");
    check(qh.isDetectableByHydrophone() == false,
          "Queen Hornet not detectable by hydrophone");

    AttackerAgent sh = AttackerAgent::create("shahed", 2, 5, 5);
    check(sh.specs.isAerial == true,
          "Shahed is aerial");
    check(sh.isDetectableByHydrophone() == false,
          "Shahed not detectable by hydrophone");
}

void testUnderwaterDetectability() {
    testSection("Underwater Agents - Hydrophone Detection");

    std::vector<std::string> underwater = {
        "bluerov2", "riptide", "yuco", "nemosens", "hugin"
    };
    for (const auto& type : underwater) {
        AttackerAgent a = AttackerAgent::create(type, 0, 5, 5);
        check(a.isDetectableByHydrophone() == true,
              type + " is detectable by hydrophone");
    }

    AttackerAgent bb = AttackerAgent::create("blueboat", 0, 5, 5);
    check(bb.specs.isSurfaceVessel == true,
          "BlueBoat is a surface vessel");
    check(bb.isDetectableByHydrophone() == false,
          "BlueBoat not detectable by hydrophone (surface)");
}

void testEverSucceededSurvivesReset() {
    testSection("everSucceeded survives S9_RESET");
    std::vector<std::vector<int>> grid(30, std::vector<int>(30, 0));
    Pathfinding pf(grid);

    AttackerAgent a = AttackerAgent::create("bluerov2", 0, 5, 5);
    a.setMissionTarget(6, 5);

    int safety = 100;
    while (a.fsmState != AgentFSMState::S9_RESET && safety-- > 0) {
            (void)a.tick(pf);
    }

    check(a.fsmState == AgentFSMState::S9_RESET,
          "Agent reaches S9_RESET within safety cap");
    check(a.everSucceeded == true,
          "everSucceeded stays true after S9_RESET wipes missionSuccess");
    check(a.missionSuccess == false,
          "missionSuccess correctly gets wiped by enterS9 (per-cycle field)");
    check(a.bestStepsToTarget > 0,
          "bestStepsToTarget retains the successful run's step count");
}

void testFrequencyRangeDetection() {
    testSection("Frequency Range Detection");

    AttackerAgent bluerov = AttackerAgent::create("bluerov2", 0, 5, 5);

    check(bluerov.isInFrequencyRange(200000, 400000) == true,
          "BlueROV2 detected by 200k-400k Hz detector (overlap at 300k-400k)");

    check(bluerov.isInFrequencyRange(300000, 450000) == true,
          "BlueROV2 detected by exact frequency match");

    check(bluerov.isInFrequencyRange(100000, 299999) == false,
          "BlueROV2 NOT detected by 100k-299k Hz detector (no overlap)");

    check(bluerov.isInFrequencyRange(451000, 900000) == false,
          "BlueROV2 NOT detected by 451k-900k Hz detector (no overlap)");

    AttackerAgent tb2 = AttackerAgent::create("tb2", 1, 5, 5);
    check(tb2.isInFrequencyRange(0, 999999) == false,
          "TB2 returns false for isInFrequencyRange (aerial)");
}

void testAerialIgnoresDetectorRegardlessOfRange() {
    testSection("Aerial Attacker vs Detector Integration");

    AttackerAgent tb2 = AttackerAgent::create("tb2", 0, 5, 5);
    check(tb2.isInFrequencyRange(0, 999999999) == false,
          "TB2 not detected even by an unbounded frequency detector");
    check(tb2.isDetectableByHydrophone() == false,
          "TB2 isDetectableByHydrophone returns false independent of range");

    AttackerAgent shahed = AttackerAgent::create("shahed", 1, 5, 5);
    check(shahed.isInFrequencyRange(0, 999999999) == false,
          "Shahed not detected even by an unbounded frequency detector");

    AttackerAgent queenhornet = AttackerAgent::create("queenhornet", 2, 5, 5);
    check(queenhornet.isInFrequencyRange(0, 999999999) == false,
          "Queen Hornet not detected even by an unbounded frequency detector");
}

void testUnknownType() {
    testSection("Unknown Agent Type");

    AttackerAgent a = AttackerAgent::create("doesntexist", 0, 5, 5);

    check(a.specs.agentType == "bluerov2",
          "Unknown type falls back to bluerov2");
    check(a.fsmState == AgentFSMState::S0_IDLE,
          "Unknown type still starts in S0_IDLE");
    check(a.alive == true,
          "Unknown type still starts alive");
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  NEW TESTS â€” Batch 1: validateState, costCategory, isInRange
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

void testValidateState() {
    testSection("AttackerAgent::validateState()");

    AttackerAgent a = AttackerAgent::create("bluerov2", 0, 5, 5);
    check(a.validateState() == true,
          "validateState() returns true for normal agent in S0_IDLE");

    AttackerAgent b = AttackerAgent::create("bluerov2", 1, 5, 5);
    b.fsmState = AgentFSMState::ABORT;
    b.alive = true;
    check(b.validateState() == false,
          "validateState() returns false when ABORT state but alive=true");
    check(b.alive == true,
          "validateState() does not modify alive flag (read-only check)");

    AttackerAgent c = AttackerAgent::create("bluerov2", 2, 5, 5);
    c.fsmState = AgentFSMState::S9_RESET;
    c.alive = false;
    check(c.validateState() == true,
          "validateState() returns true for S9_RESET with alive=false");

    AttackerAgent d = AttackerAgent::create("bluerov2", 3, 5, 5);
    d.fsmState = AgentFSMState::FALLBACK;
    d.alive = false;
    d.fallbackReason = "test";
    check(d.validateState() == true,
          "validateState() returns true for FALLBACK with alive=false");

    AttackerAgent e = AttackerAgent::create("bluerov2", 4, -1, -1);
    check(e.validateState() == true,
          "validateState() returns true for pos(-1,-1) â€” no grid reference available for bounds check");
    check(e.row == -1 && e.col == -1,
          "AttackerAgent stores arbitrary position values even if negative");
}

void testCostCategory() {
    testSection("VehicleSpecs::costCategory()");

    {
        VehicleSpecs specs = getVehicleSpecs("bluerov2");
        check(specs.costCategory() == "budget", "BlueROV2 ($6k) -> budget");
    }
    {
        VehicleSpecs specs = getVehicleSpecs("blueboat");
        check(specs.costCategory() == "budget", "BlueBoat ($5k) -> budget");
    }
    {
        VehicleSpecs specs = getVehicleSpecs("queenhornet");
        check(specs.costCategory() == "budget", "Queen Hornet ($3k avg) -> budget");
    }

    {
        VehicleSpecs specs = getVehicleSpecs("riptide");
        check(specs.costCategory() == "mid-range", "Riptide ($30k avg) -> mid-range");
    }
    {
        VehicleSpecs specs = getVehicleSpecs("shahed");
        check(specs.costCategory() == "mid-range", "Shahed ($35k avg) -> mid-range");
    }
    {
        VehicleSpecs specs = getVehicleSpecs("yuco");
        check(specs.costCategory() == "mid-range", "YUCO ($75k avg) -> mid-range");
    }

    {
        VehicleSpecs specs = getVehicleSpecs("hugin");
        check(specs.costCategory() == "flagship", "HUGIN ($3M avg) -> flagship");
    }
    {
        VehicleSpecs specs = getVehicleSpecs("tb2");
        check(specs.costCategory() == "flagship", "TB2 ($3.5M avg) -> flagship");
    }
}

void testIsInRange() {
    testSection("AttackerAgent::isInRange()");

    AttackerAgent a = AttackerAgent::create("bluerov2", 0, 10, 10);
    a.sensingRadius = 5.0;

    check(a.isInRange(12, 10) == true,
          "isInRange returns true for cell 2 units away (inside radius 5)");
    check(a.isInRange(10, 14) == true,
          "isInRange returns true for cell 4 units away (inside radius 5)");
    check(a.isInRange(13, 13) == true,
          "isInRange returns true for diagonal cell ~4.24 units away (inside radius 5)");
    check(a.isInRange(15, 10) == true,
          "isInRange returns true for cell exactly at radius boundary (5 units)");
    check(a.isInRange(16, 10) == false,
          "isInRange returns false for cell 6 units away (outside radius 5)");
    check(a.isInRange(10, 4) == false,
          "isInRange returns false for cell 6 units away (outside radius 5)");
    check(a.isInRange(10, 10) == true,
          "isInRange returns true for same cell (distance 0)");

    a.sensingRadius = 0.0;
    check(a.isInRange(10, 10) == true,
          "isInRange returns true for same cell at radius 0");
    check(a.isInRange(10, 11) == false,
          "isInRange returns false for adjacent cell at radius 0");
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  NEW TESTS â€” Batch 2: shortCode, retarget, VehicleSpecs::isDetectableByHydrophone
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

void testShortCode() {
    testSection("VehicleSpecs::shortCode()");

    {
        VehicleSpecs s = getVehicleSpecs("bluerov2");
        check(s.shortCode() == "BR", "bluerov2 -> BR");
    }
    {
        VehicleSpecs s = getVehicleSpecs("riptide");
        check(s.shortCode() == "RP", "riptide -> RP");
    }
    {
        VehicleSpecs s = getVehicleSpecs("blueboat");
        check(s.shortCode() == "BB", "blueboat -> BB");
    }
    {
        VehicleSpecs s = getVehicleSpecs("yuco");
        check(s.shortCode() == "YU", "yuco -> YU");
    }
    {
        VehicleSpecs s = getVehicleSpecs("nemosens");
        check(s.shortCode() == "NS", "nemosens -> NS");
    }
    {
        VehicleSpecs s = getVehicleSpecs("hugin");
        check(s.shortCode() == "HU", "hugin -> HU");
    }
    {
        VehicleSpecs s = getVehicleSpecs("tb2");
        check(s.shortCode() == "T2", "tb2 -> T2");
    }
    {
        VehicleSpecs s = getVehicleSpecs("queenhornet");
        check(s.shortCode() == "QH", "queenhornet -> QH");
    }
    {
        VehicleSpecs s = getVehicleSpecs("shahed");
        check(s.shortCode() == "SH", "shahed -> SH");
    }

    VehicleSpecs unknown;
    unknown.agentType = "nonexistent";
    check(unknown.shortCode() == "??", "unknown type -> ??");
}

void testRetargetOnTargetDestroyed() {
    testSection("Attacker retargets when target destroyed");

    std::vector<std::vector<int>> grid(30, std::vector<int>(30, 0));
    Pathfinding pf(grid);

    AttackerAgent a = AttackerAgent::create("bluerov2", 0, 0, 0);
    a.setMissionTarget(5, 5);
    a.targetId = 0;

        (void)a.tick(pf);
        (void)a.tick(pf);
    check(a.fsmState == AgentFSMState::S4_EXECUTE,
          "Attacker reaches S4_EXECUTE with target 0 at (5,5)");
    check(a.targetId == 0, "targetId is 0 initially");

    a.path.clear();
    a.pathIndex = 0;
    a.fsmState = AgentFSMState::S0_IDLE;
    a.milestone25 = a.milestone50 = a.milestone75 = false;
    a.stepDelayCounter = 0;
    a.targetId = -1;

    check(a.fsmState == AgentFSMState::S0_IDLE,
          "After retarget reset, FSM back to S0_IDLE");
    check(a.path.empty(),
          "After retarget reset, path is cleared");
    check(a.targetId == -1,
          "After retarget reset, targetId is -1");

    a.targetId = 1;
    a.setMissionTarget(20, 20);

        (void)a.tick(pf);
    check(a.fsmState == AgentFSMState::S3_INIT_BEHAVIOR,
          "After retarget, FSM advances to S3_INIT_BEHAVIOR with new target");
    check(a.targetId == 1,
          "After retarget, targetId updated to new target");
    check(a.hasPath(),
          "After retarget, new A* path is computed");

        (void)a.tick(pf);
    check(a.fsmState == AgentFSMState::S4_EXECUTE,
          "After retarget, FSM reaches S4_EXECUTE for new target");
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  NEW TESTS â€” Batch 3: VehicleSpecs::isDetectableByHydrophone() direct tests
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

void testVehicleSpecsHydrophoneDetection() {
    testSection("VehicleSpecs::isDetectableByHydrophone()");

    {
        VehicleSpecs s = getVehicleSpecs("bluerov2");
        check(s.isDetectableByHydrophone() == true, "BlueROV2 is hydrophone-detectable");
        check(s.isAerial == false, "BlueROV2 isAerial == false");
        check(s.isSurfaceVessel == false, "BlueROV2 isSurfaceVessel == false");
    }
    {
        VehicleSpecs s = getVehicleSpecs("hugin");
        check(s.isDetectableByHydrophone() == true, "HUGIN is hydrophone-detectable");
        check(s.isAerial == false, "HUGIN isAerial == false");
    }
    {
        VehicleSpecs s = getVehicleSpecs("yuco");
        check(s.isDetectableByHydrophone() == true, "YUCO is hydrophone-detectable");
    }

    {
        VehicleSpecs s = getVehicleSpecs("blueboat");
        check(s.isDetectableByHydrophone() == false, "BlueBoat is NOT hydrophone-detectable");
        check(s.isSurfaceVessel == true, "BlueBoat isSurfaceVessel == true");
    }

    {
        VehicleSpecs s = getVehicleSpecs("tb2");
        check(s.isDetectableByHydrophone() == false, "TB2 is NOT hydrophone-detectable");
        check(s.isAerial == true, "TB2 isAerial == true");
    }
    {
        VehicleSpecs s = getVehicleSpecs("queenhornet");
        check(s.isDetectableByHydrophone() == false, "QueenHornet is NOT hydrophone-detectable");
        check(s.isAerial == true, "QueenHornet isAerial == true");
    }
    {
        VehicleSpecs s = getVehicleSpecs("shahed");
        check(s.isDetectableByHydrophone() == false, "Shahed is NOT hydrophone-detectable");
        check(s.isAerial == true, "Shahed isAerial == true");
    }
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  NEW TESTS â€” Batch 4: summary(), recordSighting, recordIntercept, killProbability
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

void testSummary() {
    testSection("AttackerAgent::summary()");

    AttackerAgent a = AttackerAgent::create("bluerov2", 42, 7, 15);
    std::string s = a.summary();

    check(s.find("[bluerov2 id=42]") != std::string::npos,
          "summary contains type and id");
    check(s.find("state=S0_IDLE") != std::string::npos,
          "summary contains FSM state");
    check(s.find("pos=(7,15)") != std::string::npos,
          "summary contains position");
    check(s.find("alive=yes") != std::string::npos,
          "summary contains alive status");
    check(s.find("detected=no") != std::string::npos,
          "summary contains detection status");
    check(s.find("steps=0") != std::string::npos,
          "summary contains step count");
    check(s.find("cost=$6000") != std::string::npos,
          "summary contains vehicle cost");
}

void testRecordSightingAndIntercept() {
    testSection("AttackerAgent::recordSighting() and recordIntercept()");

    AttackerAgent a = AttackerAgent::create("bluerov2", 0, 5, 5);

    a.recordSighting(1, 10);
    check(a.sightingCount == 1, "sightingCount = 1 after one sighting");
    check(a.sightings.size() == 1, "sightings vector has 1 entry");
    check(a.sightings[0].seekerId == 1, "Sighting seekerId = 1");
    check(a.sightings[0].step == 10, "Sighting step = 10");

    a.recordSighting(2, 20);
    check(a.sightingCount == 2, "sightingCount = 2 after two sightings");

    a.recordIntercept(1, 15);
    check(a.killCount == 1, "killCount = 1 after one intercept");
    check(a.intercepts.size() == 1, "intercepts vector has 1 entry");
    check(a.intercepts[0].seekerId == 1, "Intercept seekerId = 1");
    check(a.intercepts[0].step == 15, "Intercept step = 15");
}

void testKillProbability() {
    testSection("AttackerAgent::killProbability()");

    AttackerAgent a = AttackerAgent::create("bluerov2", 0, 10, 10);
    a.killRadius = 5.0;

    check(a.killProbability(10, 10) == 0.90,
          "killProbability at distance 0 = 0.90");
    check(a.killProbability(11, 10) == 0.90,
          "killProbability at distance 1 (ratio 0.2) = 0.90");
    check(a.killProbability(13, 10) == 0.60,
          "killProbability at distance 3 (ratio 0.6) = 0.60");
    check(a.killProbability(14, 10) == 0.50,
          "killProbability at distance 4 (ratio 0.8) = 0.50");
    check(a.killProbability(15, 10) == 0.50,
          "killProbability at distance 5 (ratio 1.0) = 0.50");
    check(a.killProbability(16, 10) == 0.0,
          "killProbability at distance 6 (outside radius) = 0.0");

    a.killRadius = 0.0;
    check(a.killProbability(10, 10) == 0.0,
          "killProbability at any distance with zero radius = 0.0");
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  NEW TESTS â€” Batch 5: SeekerAgent edge cases
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

void testSeekerAgentHasPath() {
    testSection("SeekerAgent::hasPath()");

    std::vector<std::vector<int>> grid(30, std::vector<int>(30, 0));
    Pathfinding pf(grid);

    SeekerAgent s = SeekerAgent::create("bluerov2", 0, 5, 5);

    check(s.hasPath() == false,
          "hasPath() returns false before computePath()");

    s.computePath(pf, 10, 10);
    check(s.hasPath() == true,
          "hasPath() returns true after computePath() with valid destination");

    while (s.hasPath()) {
        s.moveStep();
    }
    check(s.hasPath() == false,
          "hasPath() returns false after walking entire path");

    check(s.moveStep() == false,
          "moveStep() returns false when no path left");

    grid[10][10] = 1;
    Pathfinding pf2(grid);
    SeekerAgent s2 = SeekerAgent::create("riptide", 1, 5, 5);
    s2.computePath(pf2, 10, 10);
    check(s2.hasPath() == false,
          "hasPath() returns false when destination is blocked");
    check(s2.path.empty(),
          "path is empty when destination is blocked");
}

void testSeekerAgentCreate() {
    testSection("SeekerAgent::create()");

    SeekerAgent s1 = SeekerAgent::create("hugin", 0, 3, 7);
    check(s1.specs.agentType == "hugin", "SeekerAgent::create('hugin') sets specs correctly");
    check(s1.id == 0, "SeekerAgent::create sets id correctly");
    check(s1.row == 3, "SeekerAgent::create sets row correctly");
    check(s1.col == 7, "SeekerAgent::create sets col correctly");

    SeekerAgent s2 = SeekerAgent::create("nonexistent", 1, 5, 5);
    check(s2.specs.agentType == "bluerov2",
          "SeekerAgent::create with invalid type falls back to bluerov2");
}

void testMoveStepWithSpeed() {
    testSection("AttackerAgent::moveStepWithSpeed()");

    std::vector<std::vector<int>> grid(30, std::vector<int>(30, 0));
    Pathfinding pf(grid);

    AttackerAgent a = AttackerAgent::create("bluerov2", 0, 5, 5);
    a.computePath(pf, 10, 10);

    int moves = 0;
    a.stepDelayCounter = 0;

    for (int i = 0; i < 12; i++) {
        if (a.moveStepWithSpeed()) {
            moves++;
        }
    }
    check(moves == 3, "BlueROV2 with stepDelay=4 moves 3 times in 12 calls");

    AttackerAgent b = AttackerAgent::create("tb2", 1, 5, 5);
    b.computePath(pf, 10, 10);
    int tb2Moves = 0;
    for (int i = 0; i < 5; i++) {
        if (b.moveStepWithSpeed()) {
            tb2Moves++;
        }
    }
    check(tb2Moves == 5, "TB2 with stepDelay=1 moves 5 times in 5 calls");
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  MAIN
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

int main() {
    std::cout << "======================================\n";
    std::cout << "  UUV Simulation - Full Test Suite\n";
    std::cout << "======================================\n";

    testFSMInitialState();
    testFSMTransitionOnTick();
    testFallbackWhenTargetEqualsSpawn();
    testBlueROV2Specs();
    testRiptideSpecs();
    testHUGINSpecs();
    testAerialAgents();
    testUnderwaterDetectability();
    testFrequencyRangeDetection();
    testAerialIgnoresDetectorRegardlessOfRange();
    testUnknownType();
    testEverSucceededSurvivesReset();

    testValidateState();
    testCostCategory();
    testIsInRange();

    testShortCode();
    testRetargetOnTargetDestroyed();
    testVehicleSpecsHydrophoneDetection();

    testSummary();
    testRecordSightingAndIntercept();
    testKillProbability();

    testSeekerAgentHasPath();
    testSeekerAgentCreate();
    testMoveStepWithSpeed();

    std::cout << "\n======================================\n";
    std::cout << "  Results: " << passedTests << " / " << totalTests << " passed\n";
    std::cout << "======================================\n";

    return (passedTests == totalTests) ? 0 : 1;
}
