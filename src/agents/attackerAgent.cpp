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

// ── Constructor ───────────────────────────────────────────────────────────────

AttackerAgent::AttackerAgent(int id, int row, int col)
    : SeekerAgent(id, row, col),
      specs(getVehicleSpecs("bluerov2")),  // default specs, will be overridden by factory
      sensingRadius(5.0), sightingCount(0),
      killRadius(3.0), killCount(0),
      fsmState(AgentFSMState::S0_IDLE), fallbackReason(""),
      stepDelayCounter(0),
      missionSuccess(false), stepsToTarget(0),
      everSucceeded(false), bestStepsToTarget(0),
      milestone25(false), milestone50(false), milestone75(false)
{
    std::cout << "[AttackerAgent id=" << id << "] S0: IDLE — awaiting spawn\n";
}

// ── Factory ───────────────────────────────────────────────────────────────────

AttackerAgent AttackerAgent::create(const std::string& type, int id, int row, int col) {
    AttackerAgent a(id, row, col);
    
    try {
        a.specs = getVehicleSpecs(type);
    } catch (const std::invalid_argument& e) {
        std::cout << "[AttackerAgent] WARNING: " << e.what() << " — using bluerov2 defaults\n";
        a.specs = getVehicleSpecs("bluerov2");
    }
    
    return a;
}

// ── stateName ─────────────────────────────────────────────────────────────────

std::string AttackerAgent::stateName() const {
    switch (fsmState) {
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
        default:                                return "UNKNOWN";
    }
}

// ── triggerFallback ───────────────────────────────────────────────────────────

void AttackerAgent::triggerFallback(const std::string& reason) {
    fsmState       = AgentFSMState::FALLBACK;
    fallbackReason = reason;
    alive          = false;
    std::cout << "[" << specs.agentType << " id=" << id
              << "] FALLBACK: " << reason << "\n";
}

// ── tick ──────────────────────────────────────────────────────────────────────

bool AttackerAgent::tick(int destRow, int destCol, const Pathfinding& pf) {
    switch (fsmState) {

        case AgentFSMState::S0_IDLE:
            std::cout << "[" << specs.agentType << " id=" << id
                      << "] S0 -> S1: waking up and accepting mission"
                      << "\n";
            enterS1(destRow, destCol);
            [[fallthrough]];

        case AgentFSMState::S1_RECEIVE_MISSION:
            enterS2(destRow, destCol);
            [[fallthrough]];

        case AgentFSMState::S2_VALIDATE: {
            // reject if target == spawn
            if (destRow == spawnRow && destCol == spawnCol) {
                triggerFallback("target is same cell as spawn");
                return true;
            }
            // log aerial detectability note
            if (specs.isAerial) {
                std::cout << "[" << specs.agentType << " id=" << id
                          << "] S2: VALIDATE — aerial agent, hydrophone detection N/A\n";
            } else {
                std::cout << "[" << specs.agentType << " id=" << id
                          << "] S2: VALIDATE — emission " << specs.emissionFreqLowHz
                          << "-" << specs.emissionFreqHighHz << " Hz, detectable by hydrophone\n";
            }
            enterS3(destRow, destCol, pf);
            break;
        }

        case AgentFSMState::S3_INIT_BEHAVIOR:
            fsmState = AgentFSMState::S4_EXECUTE;
            std::cout << "[" << specs.agentType << " id=" << id
                      << "] S4: EXECUTE — mission underway\n";
            break;

        case AgentFSMState::S4_EXECUTE: {
            runS4();
            if (row == destRow && col == destCol) reachedTarget = true; // sync immediately, don't wait for the external collision pass
            if (reachedTarget || !hasPath()) {
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

bool AttackerAgent::tick(const Pathfinding& pf) {
    if (_destRow < 0 || _destCol < 0) {
        std::cout << "[" << specs.agentType << " id=" << id
                  << "] WARNING: tick(pf) called without a mission target\n";
        return false;
    }
    return tick(_destRow, _destCol, pf);
}

void AttackerAgent::setMissionTarget(int destRow, int destCol) {
    _destRow = destRow;
    _destCol = destCol;
}

// ── FSM state implementations ─────────────────────────────────────────────────

void AttackerAgent::enterS1(int destRow, int destCol) {
    _destRow = destRow;
    _destCol = destCol;
    fsmState = AgentFSMState::S1_RECEIVE_MISSION;
    std::cout << "[" << specs.agentType << " id=" << id
              << "] S1: RECEIVE_MISSION — target=("
              << destRow << "," << destCol << ")\n";
}

void AttackerAgent::enterS2(int destRow, int destCol) {
    fsmState = AgentFSMState::S2_VALIDATE;
    std::cout << "[" << specs.agentType << " id=" << id
              << "] S2: VALIDATE — checking entry point and target\n";
}

void AttackerAgent::enterS3(int destRow, int destCol, const Pathfinding& pf) {
    fsmState = AgentFSMState::S3_INIT_BEHAVIOR;
    std::cout << "[" << specs.agentType << " id=" << id
              << "] S3: INIT_BEHAVIOR"
              << " — speed=" << specs.speedKnotsMin << "-" << specs.speedKnotsMax << " kn"
              << ", stepDelay=" << specs.stepDelay
              << ", shallow=" << (specs.shallowWaterCapable ? "yes" : "no")
              << ", cost=$" << std::fixed << std::setprecision(0) << specs.unitCostMin
              << "-$" << specs.unitCostMax << "\n";

    computePath(pf, destRow, destCol);
    std::cout << "[" << specs.agentType << " id=" << id
              << "] path length=" << path.size()
              << ", path cost=" << pathCost
              << ", nodes expanded=" << nodesExpanded << "\n";

    if (!hasPath()) {
        triggerFallback("no valid A* path to target");
    }
}

bool AttackerAgent::runS4() {
    // progress milestones based on remaining path
    if (hasPath()) {
        int total     = static_cast<int>(path.size());
        int remaining = total - pathIndex;
        float done    = (total > 0)
                      ? 1.f - static_cast<float>(remaining) / total
                      : 1.f;

        if (!milestone25 && done >= 0.25f) {
            std::cout << "[" << specs.agentType << " id=" << id
                      << "] S4: 25% of mission complete\n";
            milestone25 = true;
        }
        if (!milestone50 && done >= 0.50f) {
            std::cout << "[" << specs.agentType << " id=" << id
                      << "] S4: 50% of mission complete\n";
            milestone50 = true;
        }
        if (!milestone75 && done >= 0.75f) {
            std::cout << "[" << specs.agentType << " id=" << id
                      << "] S4: 75% of mission complete\n";
            milestone75 = true;
        }
    }
    return moveStepWithSpeed();
}

void AttackerAgent::enterS5() {
    fsmState = AgentFSMState::S5_LOG_RESULT;
    std::cout << "[" << specs.agentType << " id=" << id << "] S5: LOG_RESULT\n";
    std::cout << "    outcome   : "
              << (missionSuccess ? "TARGET REACHED" : "MISSION FAILED") << "\n";
    std::cout << "    steps     : " << stepsToTarget << "\n";
    std::cout << "    path cost : " << std::fixed << std::setprecision(4)
              << pathCost << "\n";
    std::cout << "    detected  : "
              << (detected ? "YES — first at step "
                             + std::to_string(firstDetectedAtStep)
                           : "NO") << "\n";
    std::cout << "    intercepted: " << (intercepted ? "YES" : "NO") << "\n";
}

void AttackerAgent::enterS6() {
    fsmState = AgentFSMState::S6_UPDATE_SHARED;
    std::cout << "[" << specs.agentType << " id=" << id
              << "] S6: UPDATE_SHARED — pushing to GA optimizer\n";
    std::cout << "    P(detected)    = " << (detected ? "1.0" : "0.0") << "\n";
    std::cout << "    P(intercepted) = " << (intercepted ? "1.0" : "0.0") << "\n";
    std::cout << "    success        = " << (missionSuccess ? "true" : "false") << "\n";
    std::cout << "    steps          = " << stepsToTarget << "\n";
    std::cout << "    path cost      = " << pathCost << "\n";
}

void AttackerAgent::enterS7() {
    fsmState = AgentFSMState::S7_DEACTIVATE;
    alive    = false;
    std::cout << "[" << specs.agentType << " id=" << id
              << "] S7: DEACTIVATE — agent dimmed on map\n";
}

void AttackerAgent::enterS8() {
    fsmState = AgentFSMState::S8_COMPLETE;
    std::cout << "[" << specs.agentType << " id=" << id
              << "] S8: COMPLETE — result recorded\n";
}

void AttackerAgent::enterS9() {
    fsmState         = AgentFSMState::S9_RESET;
    stepDelayCounter = 0;
    missionSuccess   = false;
    stepsToTarget    = 0;
    milestone25 = milestone50 = milestone75 = false;
    std::cout << "[" << specs.agentType << " id=" << id
              << "] S9: RESET — ready for next GA scenario\n";
}

void AttackerAgent::enterAbort() {
    fsmState = AgentFSMState::ABORT;
    alive    = false;
    std::cout << "[" << specs.agentType << " id=" << id
              << "] ABORT — agent removed from simulation\n";
}


// ── moveStepWithSpeed ─────────────────────────────────────────────────────────

bool AttackerAgent::moveStepWithSpeed() {
    stepDelayCounter++;
    if (stepDelayCounter < specs.stepDelay) return false;
    stepDelayCounter = 0;
    bool moved = moveStep();
    if (moved) {
        std::cout << "[" << specs.agentType << " id=" << id
                  << "] moved to (" << row << "," << col << ")"
                  << " step=" << stepsTaken << "\n";
    } else {
        std::cout << "[" << specs.agentType << " id=" << id
                  << "] waiting or no path available (pathIndex=" << pathIndex
                  << ", pathSize=" << path.size() << ")\n";
    }
    return moved;
}

bool AttackerAgent::isInRange(int checkRow, int checkCol) const {
    double dr = row - checkRow;
    double dc = col - checkCol;
    return std::sqrt(dr * dr + dc * dc) <= sensingRadius;
}

void AttackerAgent::recordSighting(int seekerId, int step) {
    sightings.push_back({seekerId, step});
    sightingCount++;
}

double AttackerAgent::killProbability(int checkRow, int checkCol) const {
    double dr = row - checkRow;
    double dc = col - checkCol;
    double dist = std::sqrt(dr * dr + dc * dc);
    if (dist > killRadius) return 0.0;

    double ratio = (killRadius > 0.0) ? dist / killRadius : 0.0;
    if (ratio <= 0.5) return 0.90;
    if (ratio <= 0.7) return 0.60;
    return 0.50;
}

void AttackerAgent::recordIntercept(int seekerId, int step) {
    intercepts.push_back({seekerId, step});
    killCount++;
}

void AttackerAgent::runFSMDemo(const std::string& type, const Pathfinding& pf,
                                int spawnRow, int spawnCol,
                                int targetRow, int targetCol) {
    std::cout << "\n===== FSM DEMO: " << type << " =====\n";
    AttackerAgent a = AttackerAgent::create(type, /*id=*/0, spawnRow, spawnCol);

    a.setMissionTarget(targetRow, targetCol);

    bool running = true;
    int safetyCap = 500; // avoid an infinite loop if something's wrong
    while (running && safetyCap-- > 0) {
        running = a.tick(pf);
    }

    if (safetyCap <= 0) {
        std::cout << "[runFSMDemo] WARNING: hit safety cap without reaching "
                     "a terminal state\n";
    }
    std::cout << "===== END DEMO: " << type << " =====\n\n";
}

// ── isDetectableByHydrophone ──────────────────────────────────────────────────

bool AttackerAgent::isDetectableByHydrophone() const {
    if (specs.isAerial)        return false;
    if (specs.isSurfaceVessel) return false;
    return true;
}

// ── isInFrequencyRange ────────────────────────────────────────────────────────

bool AttackerAgent::isInFrequencyRange(int detectorLowHz, int detectorHighHz) const {
    if (!isDetectableByHydrophone()) return false;
    return specs.emissionFreqLowHz  <= detectorHighHz &&
           specs.emissionFreqHighHz >= detectorLowHz;
}

// ── summary ───────────────────────────────────────────────────────────────────

std::string AttackerAgent::summary() const {
    std::ostringstream ss;
    ss << "[" << specs.agentType << " id=" << id << "]"
       << " state=" << stateName()
       << " pos=(" << row << "," << col << ")"
       << " alive=" << (alive ? "yes" : "no")
       << " detected=" << (detected ? "yes" : "no")
       << " steps=" << stepsTaken
       << " cost=$" << std::fixed << std::setprecision(0) << specs.unitCostMin;
    return ss.str();
}