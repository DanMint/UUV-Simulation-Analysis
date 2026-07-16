/**
 * test_attackerAgent.cpp
 * Author: Nadeem
 *
 * Unit tests for AttackerAgent — verifies that:
 *   1. Each vehicle type gets correct real-world specs from the factory
 *   2. Aerial agents correctly return false for isDetectableByHydrophone()
 *   3. Frequency range detection logic works correctly
 *   4. FSM starts in S0_IDLE and transitions correctly
 *   5. stepDelay is set correctly per vehicle type
 *
 * Exit codes:
 *   0 = ALL PASS
 *   1 = ONE OR MORE FAIL
 *
 * Usage:
 *   ./test_attacker
 *   (i use: cd UUV-Simulation-Analysis
 *    .\windows_build\build\Release\test_attacker.exe)
 */

#include "attackerAgent.h"
#include <iostream>
#include <string>
#include <vector>

// ── Simple test framework ─────────────────────────────────────────────────────

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

// ── Test helpers ──────────────────────────────────────────────────────────────

void testSection(const std::string& name) {
    std::cout << "\n── " << name << " ──\n";
}

// ── Tests ─────────────────────────────────────────────────────────────────────

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
    testSection("Aerial Agents — Hydrophone Detection");

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
    testSection("Underwater Agents — Hydrophone Detection");

    std::vector<std::string> underwater = {
        "bluerov2", "riptide", "yuco", "nemosens", "hugin"
    };
    for (const auto& type : underwater) {
        AttackerAgent a = AttackerAgent::create(type, 0, 5, 5);
        check(a.isDetectableByHydrophone() == true,
              type + " is detectable by hydrophone");
    }

    // BlueBoat is a surface vessel — also not hydrophone detectable
    AttackerAgent bb = AttackerAgent::create("blueboat", 0, 5, 5);
    check(bb.specs.isSurfaceVessel == true,
          "BlueBoat is a surface vessel");
    check(bb.isDetectableByHydrophone() == false,
          "BlueBoat not detectable by hydrophone (surface)");
}

void testFrequencyRangeDetection() {
    testSection("Frequency Range Detection");

    AttackerAgent bluerov = AttackerAgent::create("bluerov2", 0, 5, 5);
    // BlueROV2 emits 300k-450k Hz

    // detector covers BlueROV2 range
    check(bluerov.isInFrequencyRange(200000, 400000) == true,
          "BlueROV2 detected by 200k-400k Hz detector (overlap at 300k-400k)");

    // detector exactly matches
    check(bluerov.isInFrequencyRange(300000, 450000) == true,
          "BlueROV2 detected by exact frequency match");

    // detector too low
    check(bluerov.isInFrequencyRange(100000, 299999) == false,
          "BlueROV2 NOT detected by 100k-299k Hz detector (no overlap)");

    // detector too high
    check(bluerov.isInFrequencyRange(451000, 900000) == false,
          "BlueROV2 NOT detected by 451k-900k Hz detector (no overlap)");

    // aerial agent always returns false regardless of frequency
    AttackerAgent tb2 = AttackerAgent::create("tb2", 1, 5, 5);
    check(tb2.isInFrequencyRange(0, 999999) == false,
          "TB2 returns false for isInFrequencyRange (aerial)");
}

void testUnknownType() {
    testSection("Unknown Agent Type");

    AttackerAgent a = AttackerAgent::create("doesntexist", 0, 5, 5);
    
    // Automatically sets it to BlueROV2.
    check(a.specs.agentType == "bluerov2",
          "Unknown type falls back to bluerov2");
    check(a.fsmState == AgentFSMState::S0_IDLE,
          "Unknown type still starts in S0_IDLE");
    check(a.alive == true,
          "Unknown type still starts alive");
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "======================================\n";
    std::cout << "  AttackerAgent Unit Tests — Nadeem\n";
    std::cout << "======================================\n";

    testFSMInitialState();
    testBlueROV2Specs();
    testRiptideSpecs();
    testHUGINSpecs();
    testAerialAgents();
    testUnderwaterDetectability();
    testFrequencyRangeDetection();
    testUnknownType();

    std::cout << "\n======================================\n";
    std::cout << "  Results: " << passedTests << " / " << totalTests << " passed\n";
    std::cout << "======================================\n";

    return (passedTests == totalTests) ? 0 : 1;
}