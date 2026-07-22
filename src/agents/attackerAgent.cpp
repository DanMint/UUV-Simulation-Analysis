#include "attackerAgent.h"
#include "pathfinding.h"
#include "vehicleSpecs.h"
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cmath>
#include <iostream>
#include <array>
#include <string_view>

// ═══════════════════════════════════════════════════════════════════════════════
//  CONSTANTS
// ═══════════════════════════════════════════════════════════════════════════════

namespace {
    /// FSM state name lookup table (compile-time, O(1))
    [[nodiscard]] constexpr std::string_view kStateName(AgentFSMState state) noexcept {
        switch (state) {
            case AgentFSMState::S0_IDLE:            return "S0_IDLE";
            case AgentFSMState::S1_RECEIVE_MISSION: return "S1_RECEIVE_MISSION";
            case AgentFSMState::S2_VALIDATE:        return "S2_VALIDATE";
            case AgentFSMState::S3_INIT_BEHAVIOR:   return "S3_INIT_BEHAVIOR";
            case AgentFSMState::S4_EXECUTE:         return "S4_EXECUTE";
            case AgentFSMState::S5_LOG_RESULT:      return "S5_LOG_RESULT";
            case AgentFSMState::S6_UPDATE_SHARED:   return "S6_UPDATE_SHARED";
            case AgentFSMState::S7_DEACTIVATE:      return "S7_DEACTIVATE";
            case AgentFSMState::S8_COMPLETE:        return "S8_COMPLETE";
            case AgentFSMState::S9_RESET:           return "S9_RESET";
            case AgentFSMState::FALLBACK:           return "FALLBACK";
            case AgentFSMState::ABORT:              return "ABORT";
        }
        return "UNKNOWN";
    }

    /// Agents that can be SPAWNED (comma-separated for validation messages)
    constexpr std::string_view kValidAgentTypes =
        "bluerov2, riptide, blueboat, yuco, nemosens, hugin, tb2, queenhornet, shahed";

    /// Prefix for all log messages
    [[nodiscard]] std::string logPrefix(const std::string& agentType, int id) {
        return "[" + agentType + " id=" + std::to_string(id) + "]";
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  CONSTRUCTOR
// ═══════════════════════════════════════════════════════════════════════════════

AttackerAgent::AttackerAgent(int id, int row, int col)
    : SeekerAgent(id, row, col),
      specs(getVehicleSpecs("bluerov2"))   // safe default — overwritten by create() if called via factory
{
    std::cout << logPrefix(specs.agentType, id) << " S0: IDLE — awaiting spawn\n";
}

// ═══════════════════════════════════════════════════════════════════════════════
//  FACTORY
// ═══════════════════════════════════════════════════════════════════════════════

AttackerAgent AttackerAgent::create(const std::string& type, int id, int row, int col) {
    AttackerAgent a(id, row, col);

    try {
        a.specs = getVehicleSpecs(type);
    } catch (const std::invalid_argument& e) {
        std::cout << logPrefix(a.specs.agentType, id)
                  << " WARNING: " << e.what()
                  << " — using bluerov2 defaults\n"
                  << "    Valid types: " << kValidAgentTypes << "\n";
        a.specs = getVehicleSpecs("bluerov2");
    }

    return a;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  STATE NAME
// ═══════════════════════════════════════════════════════════════════════════════

std::string AttackerAgent::stateName() const {
    return std::string(kStateName(fsmState));
}

// ═══════════════════════════════════════════════════════════════════════════════
//  VALIDATE STATE
// ═══════════════════════════════════════════════════════════════════════════════

bool AttackerAgent::validateState() const {
    // FSM state must be a valid enum value (can't really validate enum range in C++
    // without a sentinel, but we can check for known issues).

    // alive flag consistency with terminal states
    if (fsmState == AgentFSMState::ABORT && alive) {
        std::cerr << logPrefix(specs.agentType, id)
                  << " INCONSISTENT: ABORT state but alive=true\n";
        return false;
    }

    // Check that stepDelay is positive
    if (specs.stepDelay <= 0) {
        std::cerr << logPrefix(specs.agentType, id)
                  << " INVALID: stepDelay=" << specs.stepDelay << " (must be > 0)\n";
        return false;
    }

    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  TRIGGER FALLBACK
// ═══════════════════════════════════════════════════════════════════════════════

void AttackerAgent::triggerFallback(const std::string& reason) {
    fsmState       = AgentFSMState::FALLBACK;
    fallbackReason = reason;
    alive          = false;
    std::cout << logPrefix(specs.agentType, id)
              << " FALLBACK: " << reason << "\n";
}

// ═══════════════════════════════════════════════════════════════════════════════
//  TICK — Main FSM Advancement
// ═══════════════════════════════════════════════════════════════════════════════

bool AttackerAgent::tick(int destRow, int destCol, const Pathfinding& pf) {
    switch (fsmState) {

        // ── S0: IDLE → S1 → S2 → S3 (collapsed into one tick) ────────────────
        case AgentFSMState::S0_IDLE:
            std::cout << logPrefix(specs.agentType, id)
                      << " S0 -> S1: waking up and accepting mission\n";
            enterS1(destRow, destCol);
            [[fallthrough]];

        case AgentFSMState::S1_RECEIVE_MISSION:
            enterS2(destRow, destCol);
            [[fallthrough]];

        case AgentFSMState::S2_VALIDATE: {
            // Reject if target == spawn (trivial no-op mission)
            if (destRow == spawnRow && destCol == spawnCol) {
                triggerFallback("target is same cell as spawn");
                return true;
            }

            // Log detection characteristics for analysis
            std::string detectStatus;
            if (specs.isDetectableByHydrophone()) {
                detectStatus = "emission " + std::to_string(specs.emissionFreqLowHz)
                    + "-" + std::to_string(specs.emissionFreqHighHz)
                    + " Hz, detectable by hydrophone";
            } else if (specs.isAerial) {
                detectStatus = "aerial agent, hydrophone detection N/A";
            } else {
                detectStatus = "surface vessel, hydrophone detection N/A";
            }

            std::cout << logPrefix(specs.agentType, id)
                      << " S2: VALIDATE — " << detectStatus << "\n";

            enterS3(destRow, destCol, pf);
            break;
        }

        // ── S3: INIT_BEHAVIOR → S4 (path computed, ready to execute) ──────────
        case AgentFSMState::S3_INIT_BEHAVIOR:
            fsmState = AgentFSMState::S4_EXECUTE;
            std::cout << logPrefix(specs.agentType, id)
                      << " S4: EXECUTE — mission underway\n";
            break;

        // ── S4: EXECUTE — move along path each tick ───────────────────────────
        case AgentFSMState::S4_EXECUTE: {
            runS4();

            // Check if we reached destination (sync directly, don't wait for collision pass)
            if (row == destRow && col == destCol) {
                reachedTarget = true;
            }

            if (reachedTarget || !hasPath()) {
                // Mission completed (success or path exhausted)
                missionSuccess = reachedTarget;
                stepsToTarget  = stepsTaken;
                if (reachedTarget) {
                    everSucceeded = true;
                    bestStepsToTarget = stepsTaken;
                }
                enterS5();
            } else if (!alive) {
                triggerFallback("killed by an interceptor");
            } else if (intercepted) {
                triggerFallback("intercepted at step " + std::to_string(interceptedAtStep));
            } else if (stepsTaken >= kMaxStepsBeforeAbort) {
                triggerFallback("exceeded max steps (" + std::to_string(kMaxStepsBeforeAbort) + ")");
            }
            break;
        }

        // ── S5–S8: Terminal wind-down (one tick each) ─────────────────────────
        case AgentFSMState::S5_LOG_RESULT:
            enterS6();
            break;

        case AgentFSMState::S6_UPDATE_SHARED:
            enterS7();
            break;

        case AgentFSMState::S7_DEACTIVATE:
            enterS8();
            break;

        case AgentFSMState::S8_COMPLETE:
            enterS9();
            break;

        // ── Terminal states ────────────────────────────────────────────────────
        case AgentFSMState::S9_RESET:
            return false;

        case AgentFSMState::FALLBACK:
            enterAbort();
            return false;

        case AgentFSMState::ABORT:
            return false;
    }
    return true;
}

/**
 * @brief Tick using previously-set mission target.
 *
 * Requires setMissionTarget() to have been called. If no target was set,
 * logs a warning and returns false.
 */
bool AttackerAgent::tick(const Pathfinding& pf) {
    if (_destRow < 0 || _destCol < 0) {
        std::cout << logPrefix(specs.agentType, id)
                  << " WARNING: tick(pf) called without a mission target\n";
        return false;
    }
    return tick(_destRow, _destCol, pf);
}

void AttackerAgent::setMissionTarget(int destRow, int destCol) {
    _destRow = destRow;
    _destCol = destCol;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  FSM STATE IMPLEMENTATIONS  (private — called only by tick())
// ═══════════════════════════════════════════════════════════════════════════════

void AttackerAgent::enterS1(int destRow, int destCol) {
    _destRow = destRow;
    _destCol = destCol;
    fsmState = AgentFSMState::S1_RECEIVE_MISSION;
    std::cout << logPrefix(specs.agentType, id)
              << " S1: RECEIVE_MISSION — target=("
              << destRow << "," << destCol << ")\n";
}

void AttackerAgent::enterS2(int /*destRow*/, int /*destCol*/) {
    fsmState = AgentFSMState::S2_VALIDATE;
    std::cout << logPrefix(specs.agentType, id)
              << " S2: VALIDATE — checking entry point and target\n";
}

void AttackerAgent::enterS3(int destRow, int destCol, const Pathfinding& pf) {
    fsmState = AgentFSMState::S3_INIT_BEHAVIOR;

    // Log vehicle performance characteristics
    std::cout << logPrefix(specs.agentType, id)
              << " S3: INIT_BEHAVIOR"
              << " — speed=" << specs.speedKnotsMin << "-" << specs.speedKnotsMax << " kn"
              << ", stepDelay=" << specs.stepDelay
              << ", shallow=" << (specs.shallowWaterCapable ? "yes" : "no")
              << ", cost=$" << std::fixed << std::setprecision(0) << specs.unitCostMin
              << "-$" << specs.unitCostMax
              << ", category=" << specs.costCategory() << "\n";

    // Compute A* path to target
    computePath(pf, destRow, destCol);

    std::cout << logPrefix(specs.agentType, id)
              << " path length=" << path.size()
              << ", path cost=" << pathCost
              << ", nodes expanded=" << nodesExpanded << "\n";

    if (!hasPath()) {
        triggerFallback("no valid A* path to target");
    }
}

bool AttackerAgent::runS4() {
    // Progress milestones based on path completion percentage
    if (hasPath()) {
        const int total     = static_cast<int>(path.size());
        const int remaining = total - pathIndex;
        const float done    = (total > 0)
            ? 1.0f - static_cast<float>(remaining) / static_cast<float>(total)
            : 1.0f;

        constexpr float kMilestone25 = 0.25f;
        constexpr float kMilestone50 = 0.50f;
        constexpr float kMilestone75 = 0.75f;

        auto reportMilestone = [&](bool& flag, float threshold, int pct) {
            if (!flag && done >= threshold) {
                flag = true;
                std::cout << logPrefix(specs.agentType, id)
                          << " S4: " << pct << "% of mission complete\n";
            }
        };

        reportMilestone(milestone25, kMilestone25, 25);
        reportMilestone(milestone50, kMilestone50, 50);
        reportMilestone(milestone75, kMilestone75, 75);
    }

    return moveStepWithSpeed();
}

void AttackerAgent::enterS5() {
    fsmState = AgentFSMState::S5_LOG_RESULT;
    std::cout << logPrefix(specs.agentType, id) << " S5: LOG_RESULT\n"
              << "    outcome   : " << (missionSuccess ? "TARGET REACHED" : "MISSION FAILED") << "\n"
              << "    steps     : " << stepsToTarget << "\n"
              << "    path cost : " << std::fixed << std::setprecision(4) << pathCost << "\n"
              << "    detected  : "
              << (detected ? "YES — first at step " + std::to_string(firstDetectedAtStep)
                           : "NO") << "\n"
              << "    intercepted: " << (intercepted ? "YES" : "NO") << "\n";
}

void AttackerAgent::enterS6() {
    fsmState = AgentFSMState::S6_UPDATE_SHARED;
    std::cout << logPrefix(specs.agentType, id)
              << " S6: UPDATE_SHARED — pushing to GA optimizer\n"
              << "    P(detected)    = " << (detected ? "1.0" : "0.0") << "\n"
              << "    P(intercepted) = " << (intercepted ? "1.0" : "0.0") << "\n"
              << "    success        = " << (missionSuccess ? "true" : "false") << "\n"
              << "    steps          = " << stepsToTarget << "\n"
              << "    path cost      = " << pathCost << "\n";
}

void AttackerAgent::enterS7() {
    fsmState = AgentFSMState::S7_DEACTIVATE;
    alive    = false;
    std::cout << logPrefix(specs.agentType, id)
              << " S7: DEACTIVATE — agent dimmed on map\n";
}

void AttackerAgent::enterS8() {
    fsmState = AgentFSMState::S8_COMPLETE;
    std::cout << logPrefix(specs.agentType, id)
              << " S8: COMPLETE — result recorded\n";
}

void AttackerAgent::enterS9() {
    fsmState         = AgentFSMState::S9_RESET;
    stepDelayCounter = 0;
    missionSuccess   = false;
    stepsToTarget    = 0;
    milestone25 = milestone50 = milestone75 = false;
    std::cout << logPrefix(specs.agentType, id)
              << " S9: RESET — ready for next GA scenario\n";
}

void AttackerAgent::enterAbort() {
    fsmState = AgentFSMState::ABORT;
    alive    = false;
    std::cout << logPrefix(specs.agentType, id)
              << " ABORT — agent removed from simulation\n";
}

// ═══════════════════════════════════════════════════════════════════════════════
//  MOVEMENT
// ═══════════════════════════════════════════════════════════════════════════════

bool AttackerAgent::moveStepWithSpeed() {
    stepDelayCounter++;
    if (stepDelayCounter < specs.stepDelay) return false;
    stepDelayCounter = 0;

    const bool moved = moveStep();
    if (moved) {
        std::cout << logPrefix(specs.agentType, id)
                  << " moved to (" << row << "," << col << ")"
                  << " step=" << stepsTaken << "\n";
    } else {
        std::cout << logPrefix(specs.agentType, id)
                  << " waiting or no path available (pathIndex=" << pathIndex
                  << ", pathSize=" << path.size() << ")\n";
    }
    return moved;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  DETECTION HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

bool AttackerAgent::isInRange(int checkRow, int checkCol) const {
    const double dr = static_cast<double>(row) - static_cast<double>(checkRow);
    const double dc = static_cast<double>(col) - static_cast<double>(checkCol);
    return (dr * dr + dc * dc) <= (sensingRadius * sensingRadius);
}

void AttackerAgent::recordSighting(int seekerId, int step) {
    sightings.push_back({seekerId, step});
    sightingCount++;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  KILL PROBABILITY
// ═══════════════════════════════════════════════════════════════════════════════
//
//  Distance-tiered probability model:
//    Inner 50% → 90% kill chance
//    50-70%    → 60% kill chance
//    70-100%   → 50% kill chance
//
//  This models real-world weapon system performance where closer
//  engagements have higher probability of kill (Pk).
//
// ═══════════════════════════════════════════════════════════════════════════════

double AttackerAgent::killProbability(int checkRow, int checkCol) const {
    const double dr = static_cast<double>(row) - static_cast<double>(checkRow);
    const double dc = static_cast<double>(col) - static_cast<double>(checkCol);
    const double dist = std::sqrt(dr * dr + dc * dc);

    if (dist > killRadius) return 0.0;
    if (killRadius <= 0.0) return 0.0;

    const double ratio = dist / killRadius;

    // Tiered probability: closer = deadlier
    if (ratio <= 0.5) return 0.90;   // inner zone: 90% PK
    if (ratio <= 0.7) return 0.60;   // mid zone:   60% PK
    return 0.50;                      // outer zone: 50% PK
}

void AttackerAgent::recordIntercept(int seekerId, int step) {
    intercepts.push_back({seekerId, step});
    killCount++;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  HYDROPHONE DETECTION
// ═══════════════════════════════════════════════════════════════════════════════

bool AttackerAgent::isDetectableByHydrophone() const {
    return specs.isDetectableByHydrophone();
}

bool AttackerAgent::isInFrequencyRange(int detectorLowHz, int detectorHighHz) const {
    if (!isDetectableByHydrophone()) return false;
    return specs.emissionFreqLowHz  <= detectorHighHz &&
           specs.emissionFreqHighHz >= detectorLowHz;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  SUMMARY
// ═══════════════════════════════════════════════════════════════════════════════

std::string AttackerAgent::summary() const {
    std::ostringstream ss;
    ss << logPrefix(specs.agentType, id)
       << " state=" << stateName()
       << " pos=(" << row << "," << col << ")"
       << " alive=" << (alive ? "yes" : "no")
       << " detected=" << (detected ? "yes" : "no")
       << " steps=" << stepsTaken
       << " cost=$" << std::fixed << std::setprecision(0) << specs.unitCostMin;
    return ss.str();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  FSM DEMO  (standalone demonstration tool)
// ═══════════════════════════════════════════════════════════════════════════════

void AttackerAgent::runFSMDemo(const std::string& type, const Pathfinding& pf,
                                int spawnRow, int spawnCol,
                                int targetRow, int targetCol) {
    std::cout << "\n===== FSM DEMO: " << type << " =====\n";
    AttackerAgent a = AttackerAgent::create(type, /*id=*/0, spawnRow, spawnCol);

    a.setMissionTarget(targetRow, targetCol);

    bool running = true;
    constexpr int kSafetyCap = 500;
    int safetyCounter = kSafetyCap;

    while (running && safetyCounter-- > 0) {
        running = a.tick(pf);
    }

    if (safetyCounter <= 0) {
        std::cout << "[runFSMDemo] WARNING: hit safety cap (" << kSafetyCap
                  << " iterations) without reaching a terminal state\n";
        std::cout << "  Final FSM state: " << a.stateName() << "\n";
    }

    std::cout << "===== END DEMO: " << type << " =====\n\n";
}

