#include "simulation.h"
#include "simulationRecorder.h"
#include <cmath>
#include <iomanip>
#include <limits>
#include <iostream>

Simulation::Simulation(MapCreation& map, const SpawnConfig& config, int maxSteps,
                       unsigned seed)
    : m_map(map), m_maxSteps(maxSteps),
      m_maxNoiseLevel(config.getMaxNoiseLevel()),
      m_finished(false), m_step(0), m_pf(nullptr), m_recorder(nullptr),
      m_seed(seed), m_rng(seed),
      m_detectorGrid(50.0),   // 50-cell buckets for detector queries
      m_interceptorGrid(50.0) // 50-cell buckets for interceptor queries
{
    int seekerId = 0, targetId = 0, detectorId = 0, interceptorId = 0, attackerId = 0;
    double detRadius = config.getDetectorRadius();
    double intRadius = config.getInterceptorRadius();

    for (const auto& unit : config.getUnits()) {
        if (unit.type == "seeker") {
            if (!unit.vehicleType.empty()) {
                m_seekers.push_back(SeekerAgent::create(unit.vehicleType, seekerId++, unit.row, unit.col));
            } else {
                m_seekers.emplace_back(seekerId++, unit.row, unit.col);
            }
        } else if (unit.type == "target") {
            m_targets.emplace_back(targetId++, unit.row, unit.col, unit.isCritical);
        } else if (unit.type == "detector") {
            m_detectors.emplace_back(detectorId++, unit.row, unit.col, detRadius);
        } else if (unit.type == "interceptor") {
            if (!unit.vehicleType.empty()) {
                m_interceptors.emplace_back(interceptorId++, unit.row, unit.col, intRadius, unit.vehicleType);
            } else {
                m_interceptors.emplace_back(interceptorId++, unit.row, unit.col, intRadius);
            }
        }
        else if (unit.type == "attacker") {
            if (!unit.vehicleType.empty()) {
                m_attackers.push_back(AttackerAgent::create(unit.vehicleType, attackerId++, unit.row, unit.col));
            } else {
                m_attackers.push_back(AttackerAgent::create("bluerov2", attackerId++, unit.row, unit.col));
            }
        }
    }

    std::cout << "Simulation created: "
              << m_seekers.size()      << " seekers, "
              << m_targets.size()      << " targets, "
              << m_attackers.size()    << " attackers, "
              << m_detectors.size()    << " detectors (r=" << detRadius << "), "
              << m_interceptors.size() << " interceptors (r=" << intRadius << "), "
              << "noise=" << m_maxNoiseLevel << ", "
              << "max " << m_maxSteps << " steps\n";

    if (!m_detectors.empty() && m_interceptors.empty()) {
        std::cout << "  WARNING: detectors present but no interceptors.\n";
    }
    if (m_detectors.empty() && !m_interceptors.empty()) {
        std::cout << "  WARNING: interceptors present but no detectors.\n";
    }

    // Pre-reserve result vectors to avoid reallocations during buildResult()
    m_seekers.reserve(20);
    m_targets.reserve(10);
    m_detectors.reserve(20);
    m_interceptors.reserve(20);
    m_attackers.reserve(20);
}

int Simulation::findNearestTarget(const SeekerAgent& seeker) const {
    int bestIdx = -1;
    double bestDistSq = std::numeric_limits<double>::max();
    for (int i = 0; i < static_cast<int>(m_targets.size()); i++) {
        if (!m_targets[i].alive) continue;
        double dr = seeker.row - m_targets[i].row;
        double dc = seeker.col - m_targets[i].col;
        double distSq = dr * dr + dc * dc;
        if (distSq < bestDistSq) { bestDistSq = distSq; bestIdx = i; }
    }
    return bestIdx;
}

bool Simulation::checkCollision(const SeekerAgent& seeker, const TargetAgent& target) const {
    return seeker.row == target.row && seeker.col == target.col;
}

void Simulation::updateDetectorTracks(int currentStep) {
    // Rebuild spatial index for detectors (cheap: O(D) insertions)
    m_detectorGrid.clear();
    for (int d = 0; d < static_cast<int>(m_detectors.size()); d++) {
        if (m_detectors[d].alive) {
            m_detectorGrid.insert(d, m_detectors[d].row, m_detectors[d].col);
        }
    }

    for (auto& seeker : m_seekers) {
        if (!seeker.alive || seeker.reachedTarget) continue;
        auto nearby = m_detectorGrid.query(seeker.row, seeker.col,
                                           m_detectors.empty() ? 0.0 : m_detectors[0].sensingRadius);
        for (int dIdx : nearby) {
            auto& detector = m_detectors[dIdx];
            if (!detector.isInRangeSq(seeker.row, seeker.col)) continue;
            detector.recordSighting(seeker.id, currentStep);
            if (!seeker.detected) {
                seeker.detected = true;
                seeker.firstDetectedAtStep = currentStep;
                seeker.firstDetectedByDetector = detector.id;
            }
        }
    }
    for (auto& attacker : m_attackers) {
        if (!attacker.alive || attacker.reachedTarget) continue;
        if (!attacker.isDetectableByHydrophone()) continue;
        auto nearby = m_detectorGrid.query(attacker.row, attacker.col,
                                           m_detectors.empty() ? 0.0 : m_detectors[0].sensingRadius);
        for (int dIdx : nearby) {
            auto& detector = m_detectors[dIdx];
            if (!detector.isInRangeSq(attacker.row, attacker.col)) continue;
            if (!attacker.isInFrequencyRange(detector.freqLowHz, detector.freqHighHz)) continue;
            detector.recordSighting(attacker.id, currentStep);
            attacker.recordSighting(detector.id, currentStep);
            if (!attacker.detected) {
                attacker.detected = true;
                attacker.firstDetectedAtStep = currentStep;
                attacker.firstDetectedByDetector = detector.id;
            }
        }
    }
}

void Simulation::checkInterceptorEngagements(int currentStep) {
    for (auto& seeker : m_seekers) {
        if (!seeker.alive || seeker.reachedTarget) continue;
        if (!seeker.detected) continue;
        for (auto& interceptor : m_interceptors) {
            if (!interceptor.alive) continue;
            if (!interceptor.isInRangeSq(seeker.row, seeker.col)) continue;
            interceptor.engagementCount++;
            double pKill = interceptor.killProbabilitySq(seeker.row, seeker.col, interceptor.killRadiusSq);
            double r = m_roll(m_rng);
            if (r < pKill) {
                seeker.alive = false;
                seeker.intercepted = true;
                seeker.interceptedByInterceptor = interceptor.id;
                seeker.interceptedAtStep = currentStep;
                interceptor.recordIntercept(seeker.id, currentStep);
                break;
            }
        }
    }
    for (auto& attacker : m_attackers) {
        if (!attacker.alive || attacker.reachedTarget) continue;
        if (!attacker.detected) continue;
        for (auto& interceptor : m_interceptors) {
            if (!interceptor.alive) continue;
            if (!interceptor.isInRangeSq(attacker.row, attacker.col)) continue;
            interceptor.engagementCount++;
            double pKill = interceptor.killProbabilitySq(attacker.row, attacker.col, interceptor.killRadiusSq);
            double r = m_roll(m_rng);
            if (r < pKill) {
                attacker.alive = false;
                attacker.intercepted = true;
                attacker.interceptedByInterceptor = interceptor.id;
                attacker.interceptedAtStep = currentStep;
                attacker.intercepts.push_back({interceptor.id, currentStep});
                interceptor.recordIntercept(attacker.id, currentStep);
                break;
            }
        }
    }
}

[[nodiscard]] static bool bresenhamLOS(const MapCreation& map, int r0, int c0, int r1, int c1) {
    int dr = std::abs(r1 - r0);
    int dc = std::abs(c1 - c0);
    int sr = (r0 < r1) ? 1 : -1;
    int sc = (c0 < c1) ? 1 : -1;
    int err = dc - dr;
    int r = r0, c = c0;
    while (r != r1 || c != c1) {
        int e2 = 2 * err;
        if (e2 > -dr) { err -= dr; c += sc; }
        if (e2 < dc)  { err += dc; r += sr; }
        if (!map.isValid(r, c) || !map.isPassable(r, c)) return false;
    }
    return true;
}

template <typename Agent>
bool Simulation::applyNoiseImpl(Agent& agent) {
    if (m_maxNoiseLevel <= 0.0) return false;
    if (!agent.alive || agent.reachedTarget) return false;
    std::uniform_real_distribution<double> dist(-m_maxNoiseLevel, m_maxNoiseLevel);
    int rx = static_cast<int>(std::round(dist(m_rng)));
    int ry = static_cast<int>(std::round(dist(m_rng)));
    if (rx == 0 && ry == 0) return false;
    int newRow = agent.row + ry;
    int newCol = agent.col + rx;
    if (!m_map.isValid(newRow, newCol)) return false;
    if (!m_map.isPassable(newRow, newCol)) return false;
    if (!bresenhamLOS(m_map, agent.row, agent.col, newRow, newCol)) return false;
    agent.row = newRow;
    agent.col = newCol;
    agent.moveHistory.push_back({newRow, newCol});
    agent.path.clear();
    agent.pathIndex = 0;
    return true;
}

template bool Simulation::applyNoiseImpl<SeekerAgent>(SeekerAgent&);
template bool Simulation::applyNoiseImpl<AttackerAgent>(AttackerAgent&);

[[maybe_unused]] bool Simulation::applyNoise(SeekerAgent& seeker) {
    return applyNoiseImpl(seeker);
}

void Simulation::assignTargets(const Pathfinding& pf) {
    for (auto& seeker : m_seekers) {
        if (!seeker.alive || seeker.reachedTarget) continue;
        int tIdx = findNearestTarget(seeker);
        if (tIdx < 0) continue;
        if (seeker.targetId != m_targets[tIdx].id || !seeker.hasPath()) {
            seeker.targetId = m_targets[tIdx].id;
            seeker.computePath(pf, m_targets[tIdx].row, m_targets[tIdx].col);
        }
    }
}

SimResult Simulation::buildResult(int totalSteps) const {
    SimResult result;
    result.totalSteps = totalSteps;
    result.allTargetsDestroyed = true;
    result.allSeekersDead = true;
    result.maxNoiseLevel = m_maxNoiseLevel;

    for (const auto& s : m_seekers) {
        SimResult::SeekerResult sr;
        sr.id = s.id; sr.stepsTaken = s.stepsTaken; sr.pathCost = s.pathCost;
        sr.nodesExpanded = s.nodesExpanded; sr.reachedTarget = s.reachedTarget;
        sr.targetId = s.targetId; sr.moveHistory = s.moveHistory;
        sr.detected = s.detected; sr.firstDetectedAtStep = s.firstDetectedAtStep;
        sr.firstDetectedByDetector = s.firstDetectedByDetector;
        sr.intercepted = s.intercepted; sr.interceptedByInterceptor = s.interceptedByInterceptor;
        sr.interceptedAtStep = s.interceptedAtStep;
        sr.unitCostMin = s.specs.unitCostMin;
        result.seekerResults.push_back(sr);
        if (s.alive) result.allSeekersDead = false;
    }
    for (const auto& t : m_targets) {
        SimResult::TargetResult tr;
        tr.id = t.id; tr.row = t.row; tr.col = t.col; tr.destroyed = !t.alive;
        tr.destroyedAtStep = -1; tr.destroyedBySeeker = -1;
        tr.isCritical = t.isCritical;
        result.targetResults.push_back(tr);
        if (t.alive) result.allTargetsDestroyed = false;
    }
    for (const auto& d : m_detectors) {
        SimResult::DetectorResult dr;
        dr.id = d.id; dr.row = d.row; dr.col = d.col;
        dr.sensingRadius = d.sensingRadius; dr.sightingCount = d.sightingCount;
        dr.unitCost = d.unitCost;
        for (const auto& s : d.sightings) dr.sightings.push_back({s.seekerId, s.step});
        result.detectorResults.push_back(dr);
    }
    for (const auto& i : m_interceptors) {
        SimResult::InterceptorResult ir;
        ir.id = i.id; ir.row = i.row; ir.col = i.col;
        ir.killRadius = i.killRadius; ir.killCount = i.killCount;
        ir.engagementCount = i.engagementCount;
        ir.engagementCost = i.engagementCost;
        ir.unitCost = i.unitCost;
        ir.vehicleType = i.vehicleType;
        for (const auto& ic : i.intercepts) ir.intercepts.push_back({ic.seekerId, ic.step});
        result.interceptorResults.push_back(ir);
    }
    for (const auto& a : m_attackers) {
        SimResult::AttackerResult ar;
        ar.id = a.id; ar.row = a.row; ar.col = a.col; ar.alive = a.alive;
        ar.state = a.stateName(); ar.agentType = a.specs.agentType;
        ar.missionSuccess = a.everSucceeded;
        ar.stepsTaken = a.stepsTaken; ar.pathCost = a.pathCost;
        ar.nodesExpanded = a.nodesExpanded; ar.targetId = a.targetId; ar.killCount = a.killCount;
        for (const auto& s : a.sightings) ar.sightings.push_back({s.seekerId, s.step});
        for (const auto& ic : a.intercepts) ar.intercepts.push_back({ic.seekerId, ic.step});
        ar.moveHistory = a.moveHistory;
        ar.unitCostMin = a.specs.unitCostMin;
        result.attackerResults.push_back(ar);
    }
    result.computeSummary();
    return result;
}

void Simulation::updateAttackerStates(int currentStep, const Pathfinding& pf) {
    for (auto& attacker : m_attackers) {
        if (attacker.fsmState == AgentFSMState::S9_RESET) continue;
        if (!attacker.alive) { attacker.tick(pf); continue; }
        if (attacker.targetId >= 0 &&
            attacker.targetId < static_cast<int>(m_targets.size()) &&
            !m_targets[attacker.targetId].alive) {
            attacker.targetId = -1;
            attacker.path.clear();
            attacker.pathIndex = 0;
            attacker.fsmState = AgentFSMState::S0_IDLE;
            attacker.milestone25 = attacker.milestone50 = attacker.milestone75 = false;
            attacker.stepDelayCounter = 0;
        }
        int bestTarget = -1;
        double bestDistSq = std::numeric_limits<double>::max();
        for (int i = 0; i < static_cast<int>(m_targets.size()); i++) {
            if (!m_targets[i].alive) continue;
            double dr = attacker.row - m_targets[i].row;
            double dc = attacker.col - m_targets[i].col;
            double distSq = dr * dr + dc * dc;
            if (distSq < bestDistSq) { bestDistSq = distSq; bestTarget = i; }
        }
        if (bestTarget < 0) continue;
        attacker.targetId = m_targets[bestTarget].id;
        attacker.setMissionTarget(m_targets[bestTarget].row, m_targets[bestTarget].col);
        attacker.tick(pf);
    }
}

void Simulation::executeOneStepInternal(int step, bool verbose) {
    for (auto& seeker : m_seekers) {
        if (!seeker.alive || seeker.reachedTarget) continue;
        seeker.moveStep();
    }
    updateAttackerStates(step, *m_pf);
    if (m_maxNoiseLevel > 0.0) {
        for (auto& seeker : m_seekers) applyNoise(seeker);
        for (auto& attacker : m_attackers) applyNoiseImpl(attacker);
    }
    updateDetectorTracks(step);
    checkInterceptorEngagements(step);
    for (auto& seeker : m_seekers) {
        if (!seeker.alive || seeker.reachedTarget) continue;
        for (auto& target : m_targets) {
            if (!target.alive) continue;
            if (checkCollision(seeker, target)) {
                target.alive = false;
                seeker.reachedTarget = true;
                break;
            }
        }
    }
    for (auto& attacker : m_attackers) {
        if (!attacker.alive) continue;
        for (auto& target : m_targets) {
            if (!target.alive) continue;
            if (attacker.row == target.row && attacker.col == target.col) {
                target.alive = false;
                attacker.reachedTarget = true;
                break;
            }
        }
    }
    bool needsRetarget = false;
    for (const auto& seeker : m_seekers) {
        if (!seeker.alive || seeker.reachedTarget) continue;
        if (seeker.targetId >= 0 && seeker.targetId < static_cast<int>(m_targets.size())) {
            if (!m_targets[seeker.targetId].alive) { needsRetarget = true; break; }
        }
        if (!seeker.hasPath()) { needsRetarget = true; break; }
    }
    if (needsRetarget) assignTargets(*m_pf);

    // ── Termination checks ───────────────────────────────────────────

    // Condition A: All targets destroyed
    bool anyAliveTarget = false;
    for (const auto& t : m_targets) {
        if (t.alive) { anyAliveTarget = true; break; }
    }
    if (!anyAliveTarget) {
        m_finished = true;
        return;
    }

    // Condition B: All seekers finished (dead, reached target, or no path)
    bool allSeekersFinished = true;
    for (const auto& s : m_seekers) {
        if (s.alive && !s.reachedTarget) {
            if (s.hasPath()) { allSeekersFinished = false; break; }
        }
    }

    // Condition C: All attackers finished
    bool allAttackersFinished = true;
    for (const auto& a : m_attackers) {
        if (!a.alive) continue;
        if (a.reachedTarget) continue;
        if (a.fsmState == AgentFSMState::S7_DEACTIVATE) continue;
        if (a.fsmState == AgentFSMState::S8_COMPLETE) continue;
        if (a.fsmState == AgentFSMState::S9_RESET) continue;
        if (a.fsmState == AgentFSMState::FALLBACK) continue;
        if (a.fsmState == AgentFSMState::ABORT) continue;
        allAttackersFinished = false;
        break;
    }

    if (allSeekersFinished && allAttackersFinished)
        m_finished = true;
}

SimResult Simulation::runFromCurrentState() {
    // Ensure m_pf exists (lazy-init)
    if (!m_pf) {
        m_pf = new Pathfinding(m_map.getGrid());
        assignTargets(*m_pf);
    }

    // Step from current position, no reset
    while (!m_finished && m_step < m_maxSteps) {
        m_step++;
        if (m_step % 50 == 0) {
            int alive = 0;
            for (const auto& t : m_targets) if (t.alive) alive++;
            std::cout << "[HEARTBEAT] Step: " << m_step
                      << " | Attackers: " << m_attackers.size()
                      << " | Targets alive: " << alive << std::endl;
        }
        executeOneStepInternal(m_step, false);
    }

    int finalStep = m_step;
    std::cout << "--- Simulation finished at step " << finalStep << " ---\n";
    SimResult result = buildResult(finalStep);
    result.computeSummary();
    return result;
}

void Simulation::finishFromCurrentState() {
    while (!m_finished && m_step < m_maxSteps) {
        stepOnce();
    }
}

bool Simulation::stepOnce() {
    if (m_finished) return false;
    if (!m_pf) {
        m_pf = new Pathfinding(m_map.getGrid());
        assignTargets(*m_pf);
    }
    m_step++;
    if (m_step > m_maxSteps) { m_finished = true; return false; }
    executeOneStepInternal(m_step, true);
    if (m_recorder) m_recorder->recordStep(*this, m_step);
    return !m_finished;
}

SimResult Simulation::run() {
    std::cout << "\n--- Simulation starting (headless) ---\n";
    m_finished = false;
    m_step = 0;
    // Prevent a leak if run() is called more than once on the same object.
    delete m_pf;
    m_pf = new Pathfinding(m_map.getGrid());
    assignTargets(*m_pf);

    while (!m_finished && m_step < m_maxSteps) {
        m_step++;
        if (m_step % 50 == 0) {
            int alive = 0;
            for (const auto& t : m_targets) if (t.alive) alive++;
            std::cout << "[HEARTBEAT] Step: " << m_step
                      << " | Attackers: " << m_attackers.size()
                      << " | Targets alive: " << alive << std::endl;
        }
        executeOneStepInternal(m_step, false);
        if (m_recorder) m_recorder->recordStep(*this, m_step);
    }

    int finalStep = m_step;
    std::cout << "--- Simulation finished at step " << finalStep << " ---\n";

    std::cout << "\n\n==========================================\n";
    std::cout << "           MISSION FINAL REPORT           \n";
    std::cout << "==========================================\n";
    std::cout << "Total Steps: " << finalStep << "\n";

    for (const auto& a : m_attackers) {
        std::cout << "Agent " << a.id << " (" << a.specs.agentType << ") "
                  << "Result: " << (a.everSucceeded ? "SUCCESS" : "FAILED")
                  << " | Steps: " << a.stepsTaken << "\n";
    }
    std::cout << "==========================================\n";

    SimResult result = buildResult(finalStep);
    for (auto& tr : result.targetResults) {
        if (tr.destroyed) {
            for (const auto& sr : result.seekerResults) {
                if (sr.reachedTarget && sr.targetId == tr.id) {
                    tr.destroyedBySeeker = sr.id;
                    tr.destroyedAtStep = sr.stepsTaken;
                    break;
                }
            }
        }
    }
    result.computeSummary();
    return result;
}

// ════════════════════════════════════════════════════════════════════════════════
//  BATCH API FOR GA DIRECT INTEGRATION
// ════════════════════════════════════════════════════════════════════════════════

std::vector<SimResult> Simulation::runBatch(const std::vector<SpawnConfig>& configs,
                                             int maxSteps, unsigned baseSeed) {
    std::vector<SimResult> results;
    results.reserve(configs.size());

    for (size_t i = 0; i < configs.size(); i++) {
        unsigned seed = (baseSeed != 0) ? baseSeed + static_cast<unsigned>(i) : 0;
        MapCreation map = MapCreation::fromGridData(
            configs[i].getGrid(),
            configs[i].getMapInfo().cellsN,
            configs[i].getMapInfo().canvasWidth,
            configs[i].getMapInfo().canvasHeight);

        for (const auto& unit : configs[i].getUnits()) {
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

        Simulation sim(map, configs[i], maxSteps, seed);
        results.push_back(sim.run());
    }

    return results;
}

size_t Simulation::getRecorderStepCount() const {
    return m_recorder ? m_recorder->stepCount() : 0;
}

bool Simulation::saveRecording(const std::string& filepath) const {
    return m_recorder ? m_recorder->saveJSON(filepath) : false;
}

