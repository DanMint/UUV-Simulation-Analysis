#include "attackerAgent.h"
#include "pathfinding.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cmath>
#include <iostream>

// ── Constructor ───────────────────────────────────────────────────────────────

AttackerAgent::AttackerAgent(int id, int row, int col)
    : SeekerAgent(id, row, col),
      agentType("unknown"), manufacturer("unknown"),
      speedKnotsMin(0.f), speedKnotsMax(0.f),
      emissionFreqLowHz(0), emissionFreqHighHz(0),
      shallowWaterCapable(false), isAerial(false), isSurfaceVessel(false),
      unitCostMin(0.f), unitCostMax(0.f),
      sensingRadius(5.0), sightingCount(0),
      killRadius(3.0), killCount(0),
      fsmState(AgentFSMState::S0_IDLE), fallbackReason(""),
      stepDelay(1), stepDelayCounter(0),
      missionSuccess(false), stepsToTarget(0),
      milestone25(false), milestone50(false), milestone75(false)
{
    std::cout << "[AttackerAgent id=" << id << "] S0: IDLE — awaiting spawn\n";
}

// ── Factory ───────────────────────────────────────────────────────────────────

AttackerAgent AttackerAgent::create(const std::string& type, int id, int row, int col) {
    std::string t = type;
    std::transform(t.begin(), t.end(), t.begin(), ::tolower);

    AttackerAgent a(id, row, col);
    a.agentType = t;

    // UUVs
    if (t == "bluerov2") {
        a.manufacturer = "Blue Robotics";
        a.speedKnotsMin = 1.f;  a.speedKnotsMax = 3.f;
        a.emissionFreqLowHz = 300000; a.emissionFreqHighHz = 450000;
        a.shallowWaterCapable = true;
        a.isAerial = false; a.isSurfaceVessel = false;
        a.unitCostMin = 6000.f; a.unitCostMax = 6000.f;
        a.stepDelay = 3;
    }
    else if (t == "riptide") {
        a.manufacturer = "BAE Systems / Riptide";
        a.speedKnotsMin = 2.f;  a.speedKnotsMax = 5.f;
        a.emissionFreqLowHz = 200000; a.emissionFreqHighHz = 400000;
        a.shallowWaterCapable = true;
        a.isAerial = false; a.isSurfaceVessel = false;
        a.unitCostMin = 15000.f; a.unitCostMax = 45000.f;
        a.stepDelay = 2;
    }
    else if (t == "blueboat") {
        a.manufacturer = "Blue Robotics";
        a.speedKnotsMin = 2.f;  a.speedKnotsMax = 6.f;
        a.emissionFreqLowHz = 450000; a.emissionFreqHighHz = 650000;
        a.shallowWaterCapable = true;
        a.isAerial = false; a.isSurfaceVessel = true;
        a.unitCostMin = 5000.f; a.unitCostMax = 5000.f;
        a.stepDelay = 2;
    }
    else if (t == "yuco") {
        a.manufacturer = "Seaber";
        a.speedKnotsMin = 2.f;  a.speedKnotsMax = 6.f;
        a.emissionFreqLowHz = 300000; a.emissionFreqHighHz = 600000;
        a.shallowWaterCapable = true;
        a.isAerial = false; a.isSurfaceVessel = false;
        a.unitCostMin = 50000.f; a.unitCostMax = 100000.f;
        a.stepDelay = 2;
    }
    else if (t == "nemosens") {
        a.manufacturer = "RTSYS";
        a.speedKnotsMin = 2.f;  a.speedKnotsMax = 4.f;
        a.emissionFreqLowHz = 200000; a.emissionFreqHighHz = 500000;
        a.shallowWaterCapable = true;
        a.isAerial = false; a.isSurfaceVessel = false;
        a.unitCostMin = 60000.f; a.unitCostMax = 115000.f;
        a.stepDelay = 2;
    }
    else if (t == "hugin") {
        a.manufacturer = "Kongsberg";
        a.speedKnotsMin = 2.f;  a.speedKnotsMax = 5.f;
        a.emissionFreqLowHz = 200000; a.emissionFreqHighHz = 400000;
        a.shallowWaterCapable = true;
        a.isAerial = false; a.isSurfaceVessel = false;
        a.unitCostMin = 2000000.f; a.unitCostMax = 4000000.f;
        a.stepDelay = 2;
    }
    // UAVs
    else if (t == "tb2") {
        a.manufacturer = "Baykar Technologies";
        a.speedKnotsMin = 90.f; a.speedKnotsMax = 110.f;
        a.emissionFreqLowHz = 0; a.emissionFreqHighHz = 0;
        a.shallowWaterCapable = false;
        a.isAerial = true; a.isSurfaceVessel = false;
        a.unitCostMin = 2000000.f; a.unitCostMax = 5000000.f;
        a.stepDelay = 1;
    }
    else if (t == "queenhornet") {
        a.manufacturer = "Wild Hornets";
        a.speedKnotsMin = 38.f; a.speedKnotsMax = 43.f;
        a.emissionFreqLowHz = 0; a.emissionFreqHighHz = 0;
        a.shallowWaterCapable = false;
        a.isAerial = true; a.isSurfaceVessel = false;
        a.unitCostMin = 1000.f; a.unitCostMax = 5000.f;
        a.stepDelay = 1;
    }
    else if (t == "shahed") {
        a.manufacturer = "HESA (Iran)";
        a.speedKnotsMin = 90.f; a.speedKnotsMax = 100.f;
        a.emissionFreqLowHz = 0; a.emissionFreqHighHz = 0;
        a.shallowWaterCapable = false;
        a.isAerial = true; a.isSurfaceVessel = false;
        a.unitCostMin = 20000.f; a.unitCostMax = 50000.f;
        a.stepDelay = 1;
    }
    else {
        std::cout << "[AttackerAgent] WARNING: unknown type '" << type
                  << "' — default specs applied\n";
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
    std::cout << "[" << agentType << " id=" << id
              << "] FALLBACK: " << reason << "\n";
}

// ── tick ──────────────────────────────────────────────────────────────────────

bool AttackerAgent::tick(int destRow, int destCol, const Pathfinding& pf) {
    switch (fsmState) {

        case AgentFSMState::S0_IDLE:
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
            if (isAerial) {
                std::cout << "[" << agentType << " id=" << id
                          << "] S2: VALIDATE — aerial agent, hydrophone detection N/A\n";
            } else {
                std::cout << "[" << agentType << " id=" << id
                          << "] S2: VALIDATE — emission " << emissionFreqLowHz
                          << "-" << emissionFreqHighHz << " Hz, detectable by hydrophone\n";
            }
            enterS3(destRow, destCol, pf);
            break;
        }

        case AgentFSMState::S3_INIT_BEHAVIOR:
            fsmState = AgentFSMState::S4_EXECUTE;
            std::cout << "[" << agentType << " id=" << id
                      << "] S4: EXECUTE — mission underway\n";
            break;

        case AgentFSMState::S4_EXECUTE: {
            runS4();
            if (reachedTarget || !hasPath()) {
                missionSuccess = reachedTarget;
                stepsToTarget  = stepsTaken;
                enterS5();
            } else if (intercepted) {
                triggerFallback("intercepted at step "
                                + std::to_string(interceptedAtStep));
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

// ── FSM state implementations ─────────────────────────────────────────────────

void AttackerAgent::enterS1(int destRow, int destCol) {
    _destRow = destRow;
    _destCol = destCol;
    fsmState = AgentFSMState::S1_RECEIVE_MISSION;
    std::cout << "[" << agentType << " id=" << id
              << "] S1: RECEIVE_MISSION — target=("
              << destRow << "," << destCol << ")\n";
}

void AttackerAgent::enterS2(int destRow, int destCol) {
    fsmState = AgentFSMState::S2_VALIDATE;
    std::cout << "[" << agentType << " id=" << id
              << "] S2: VALIDATE — checking entry point and target\n";
}

void AttackerAgent::enterS3(int destRow, int destCol, const Pathfinding& pf) {
    fsmState = AgentFSMState::S3_INIT_BEHAVIOR;
    std::cout << "[" << agentType << " id=" << id
              << "] S3: INIT_BEHAVIOR"
              << " — speed=" << speedKnotsMin << "-" << speedKnotsMax << " kn"
              << ", stepDelay=" << stepDelay
              << ", shallow=" << (shallowWaterCapable ? "yes" : "no")
              << ", cost=$" << std::fixed << std::setprecision(0) << unitCostMin
              << "-$" << unitCostMax << "\n";

    computePath(pf, destRow, destCol);

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
            std::cout << "[" << agentType << " id=" << id
                      << "] S4: 25% of mission complete\n";
            milestone25 = true;
        }
        if (!milestone50 && done >= 0.50f) {
            std::cout << "[" << agentType << " id=" << id
                      << "] S4: 50% of mission complete\n";
            milestone50 = true;
        }
        if (!milestone75 && done >= 0.75f) {
            std::cout << "[" << agentType << " id=" << id
                      << "] S4: 75% of mission complete\n";
            milestone75 = true;
        }
    }
    return moveStepWithSpeed();
}

void AttackerAgent::enterS5() {
    fsmState = AgentFSMState::S5_LOG_RESULT;
    std::cout << "[" << agentType << " id=" << id << "] S5: LOG_RESULT\n";
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
    std::cout << "[" << agentType << " id=" << id
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
    std::cout << "[" << agentType << " id=" << id
              << "] S7: DEACTIVATE — agent dimmed on map\n";
}

void AttackerAgent::enterS8() {
    fsmState = AgentFSMState::S8_COMPLETE;
    std::cout << "[" << agentType << " id=" << id
              << "] S8: COMPLETE — result recorded\n";
}

void AttackerAgent::enterS9() {
    fsmState         = AgentFSMState::S9_RESET;
    stepDelayCounter = 0;
    missionSuccess   = false;
    stepsToTarget    = 0;
    milestone25 = milestone50 = milestone75 = false;
    std::cout << "[" << agentType << " id=" << id
              << "] S9: RESET — ready for next GA scenario\n";
}

void AttackerAgent::enterAbort() {
    fsmState = AgentFSMState::ABORT;
    alive    = false;
    std::cout << "[" << agentType << " id=" << id
              << "] ABORT — agent removed from simulation\n";
}


// ── moveStepWithSpeed ─────────────────────────────────────────────────────────

bool AttackerAgent::moveStepWithSpeed() {
    stepDelayCounter++;
    if (stepDelayCounter < stepDelay) return false;
    stepDelayCounter = 0;
    return moveStep();
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

// ── isDetectableByHydrophone ──────────────────────────────────────────────────

bool AttackerAgent::isDetectableByHydrophone() const {
    if (isAerial)        return false;
    if (isSurfaceVessel) return false;
    return true;
}

// ── isInFrequencyRange ────────────────────────────────────────────────────────

bool AttackerAgent::isInFrequencyRange(int detectorLowHz, int detectorHighHz) const {
    if (!isDetectableByHydrophone()) return false;
    return emissionFreqLowHz  <= detectorHighHz &&
           emissionFreqHighHz >= detectorLowHz;
}

// ── summary ───────────────────────────────────────────────────────────────────

std::string AttackerAgent::summary() const {
    std::ostringstream ss;
    ss << "[" << agentType << " id=" << id << "]"
       << " state=" << stateName()
       << " pos=(" << row << "," << col << ")"
       << " alive=" << (alive ? "yes" : "no")
       << " detected=" << (detected ? "yes" : "no")
       << " steps=" << stepsTaken
       << " cost=$" << std::fixed << std::setprecision(0) << unitCostMin;
    return ss.str();
}