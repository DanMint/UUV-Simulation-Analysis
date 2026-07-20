#include "simulation.h"
#include <cmath>
#include <iomanip>
#include <limits>
#include <iostream>

// ════════════════════════════════════════════════════════════════════════════════
//  CONSTRUCTOR
// ════════════════════════════════════════════════════════════════════════════════

Simulation::Simulation(MapCreation& map, const SpawnConfig& config, int maxSteps)
    : m_map(map), m_maxSteps(maxSteps),
      m_maxNoiseLevel(config.getMaxNoiseLevel()),
      m_rng(std::random_device{}())
{
    int seekerId = 0, targetId = 0, detectorId = 0, interceptorId = 0, hunterId = 0;
    double detRadius = config.getDetectorRadius();
    double intRadius = config.getInterceptorRadius();

    for (const auto& unit : config.getUnits()) {
        if (unit.type == "seeker") {
            m_seekers.emplace_back(seekerId++, unit.row, unit.col);
        } else if (unit.type == "hunter") {
            m_hunters.emplace_back(hunterId++, unit.row, unit.col);
        } else if (unit.type == "target") {
            m_targets.emplace_back(targetId++, unit.row, unit.col);
        } else if (unit.type == "detector") {
            m_detectors.emplace_back(detectorId++, unit.row, unit.col, detRadius);
        } else if (unit.type == "interceptor") {
            m_interceptors.emplace_back(interceptorId++, unit.row, unit.col, intRadius);
        }
    }

    std::cout << "Simulation created: "
              << m_seekers.size()      << " seekers, "
              << m_hunters.size()      << " hunters, "
              << m_targets.size()      << " targets, "
              << m_detectors.size()    << " detectors (r=" << detRadius << "), "
              << m_interceptors.size() << " interceptors (r=" << intRadius << "), "
              << "noise=" << m_maxNoiseLevel << ", "
              << "max " << m_maxSteps << " steps\n";

    // Friendly warnings about asymmetric defender setups
    if (!m_detectors.empty() && m_interceptors.empty()) {
        std::cout << "  WARNING: detectors present but no interceptors. "
                  << "Seekers will be tracked but never killed.\n";
    }
    if (m_detectors.empty() && !m_interceptors.empty()) {
        std::cout << "  WARNING: interceptors present but no detectors. "
                  << "Under sense-then-shoot doctrine, no seeker can be tracked, "
                  << "so interceptors will never fire.\n";
    }
}

// ════════════════════════════════════════════════════════════════════════════════
//  FIND NEAREST TARGET
// ════════════════════════════════════════════════════════════════════════════════

int Simulation::findNearestTarget(const SeekerAgent& seeker) const {
    int bestIdx = -1;
    double bestDist = std::numeric_limits<double>::max();

    for (int i = 0; i < static_cast<int>(m_targets.size()); i++) {
        if (!m_targets[i].alive) continue;


        double dr = seeker.row - m_targets[i].row;
        double dc = seeker.col - m_targets[i].col;
        double dist = std::sqrt(dr * dr + dc * dc);

        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }
    return bestIdx;
}

// ════════════════════════════════════════════════════════════════════════════════
//  COLLISION CHECK
// ════════════════════════════════════════════════════════════════════════════════

bool Simulation::checkCollision(const SeekerAgent& seeker, const TargetAgent& target) const {
    return seeker.row == target.row && seeker.col == target.col;
}

bool Simulation::checkHunterCapture(const HunterAgent& hunter, const SeekerAgent& seeker) const {
    if (!hunter.alive || !seeker.alive) return false;

    bool sameCell = hunter.row == seeker.row && hunter.col == seeker.col;
    bool adjacent = std::abs(hunter.row - seeker.row) <= 1 && std::abs(hunter.col - seeker.col) <= 1;
    return sameCell || adjacent;
}

// ════════════════════════════════════════════════════════════════════════════════
//  SENSE PHASE — DETECTORS UPDATE TRACKS
// ════════════════════════════════════════════════════════════════════════════════
//
//  For every alive seeker:
//    - For every alive detector whose sensingRadius contains the seeker:
//        * Log a sighting (every step the seeker is inside).
//        * On the FIRST detection, mark the seeker as tracked
//          (sticky — stays tracked for the rest of the run).
//
//  Detectors do not kill anything. Killing is the interceptor's job.
//
// ════════════════════════════════════════════════════════════════════════════════

void Simulation::updateDetectorTracks(int currentStep) {
    for (auto& seeker : m_seekers) {
        if (!seeker.alive || seeker.reachedTarget) continue;

        for (auto& detector : m_detectors) {
            if (!detector.alive) continue;
            if (!detector.isInRange(seeker.row, seeker.col)) continue;

            // Log this sighting unconditionally (analysis can de-dup later)
            detector.recordSighting(seeker.id, currentStep);

            // First detection: mark seeker as tracked
            if (!seeker.detected) {
                seeker.detected = true;
                seeker.firstDetectedAtStep = currentStep;
                seeker.firstDetectedByDetector = detector.id;
                std::cout << "  Step " << currentStep
                          << ": Detector " << detector.id
                          << " acquired Seeker " << seeker.id
                          << " at (" << seeker.row << "," << seeker.col << ")\n";
            }
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════════
//  SHOOT PHASE — INTERCEPTORS ENGAGE TRACKED SEEKERS
// ════════════════════════════════════════════════════════════════════════════════
//
//  Sense-then-shoot doctrine: an interceptor will only roll a kill
//  against a seeker that has been detected by some detector.
//  Untracked seekers are skipped entirely.
//
//  The kill probability comes from the interceptor's own distance-tiered
//  model (see interceptorAgent.h):
//      inner 50% of radius -> 90%, 50-70% -> 60%, 70-100% -> 50%.
//
// ════════════════════════════════════════════════════════════════════════════════

void Simulation::checkInterceptorEngagements(int currentStep) {
    std::uniform_real_distribution<double> roll(0.0, 1.0);

    for (auto& seeker : m_seekers) {
        if (!seeker.alive || seeker.reachedTarget) continue;
        if (!seeker.detected) continue;  // <-- core doctrine: no track, no shot

        for (auto& interceptor : m_interceptors) {
            if (!interceptor.alive) continue;
            if (!interceptor.isInRange(seeker.row, seeker.col)) continue;

            double pKill = interceptor.killProbability(seeker.row, seeker.col);
            double r = roll(m_rng);

            // For logging only
            double dr = interceptor.row - seeker.row;
            double dc = interceptor.col - seeker.col;
            double dist = std::sqrt(dr * dr + dc * dc);
            double ratio = (interceptor.killRadius > 0.0)
                ? dist / interceptor.killRadius : 0.0;

            if (r < pKill) {
                std::cout << "  Step " << currentStep
                          << ": Interceptor " << interceptor.id
                          << " killed Seeker " << seeker.id
                          << " at (" << seeker.row << "," << seeker.col << ")"
                          << " [dist=" << std::fixed << std::setprecision(1)
                          << (ratio * 100) << "%, p=" << (pKill * 100) << "%]\n";

                seeker.alive = false;
                seeker.intercepted = true;
                seeker.interceptedByInterceptor = interceptor.id;
                seeker.interceptedAtStep = currentStep;

                interceptor.recordIntercept(seeker.id, currentStep);
                break;  // seeker is dead, stop checking other interceptors
            } else {
                std::cout << "  Step " << currentStep
                          << ": Interceptor " << interceptor.id
                          << " missed Seeker " << seeker.id
                          << " [dist=" << std::fixed << std::setprecision(1)
                          << (ratio * 100) << "%, p=" << (pKill * 100) << "%]\n";
            }
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════════
//  APPLY NOISE  (unchanged from the previous version)
// ════════════════════════════════════════════════════════════════════════════════

bool Simulation::applyNoise(SeekerAgent& seeker) {
    if (m_maxNoiseLevel <= 0.0) return false;
    if (!seeker.alive || seeker.reachedTarget) return false;

    std::uniform_real_distribution<double> dist(-m_maxNoiseLevel, m_maxNoiseLevel);
    int rx = static_cast<int>(std::round(dist(m_rng)));
    int ry = static_cast<int>(std::round(dist(m_rng)));

    if (rx == 0 && ry == 0) return false;

    int newRow = seeker.row + ry;
    int newCol = seeker.col + rx;

    if (!m_map.isValid(newRow, newCol)) return false;
    if (!m_map.isPassable(newRow, newCol)) return false;

    // ── Bresenham line-of-sight check: reject if any cell along the
    //    displacement is blocked (prevents teleporting over land). ──
    {
        int r0 = seeker.row, c0 = seeker.col;
        int r1 = newRow, c1 = newCol;
        int dr = std::abs(r1 - r0);
        int dc = std::abs(c1 - c0);
        int sr = (r0 < r1) ? 1 : -1;
        int sc = (c0 < c1) ? 1 : -1;
        int err = dc - dr;

        int r = r0, c = c0;
        while (r != r1 || c != c1) {
            int e2 = 2 * err;
            if (e2 > -dr) { err -= dr; c += sc; }
            if (e2 <  dc) { err += dc; r += sr; }

            if (!m_map.isValid(r, c) || !m_map.isPassable(r, c)) {
                return false;
            }
        }
    }

    seeker.row = newRow;
    seeker.col = newCol;
    seeker.moveHistory.push_back({newRow, newCol});

    // Invalidate path — must be recomputed from new position
    seeker.path.clear();
    seeker.pathIndex = 0;
    return true;
}

// ════════════════════════════════════════════════════════════════════════════════
//  ASSIGN TARGETS  (unchanged)
// ════════════════════════════════════════════════════════════════════════════════

void Simulation::assignTargets(const Pathfinding& pf) {
    std::vector<bool> targetAssigned(m_targets.size(), false);

    for (auto& seeker : m_seekers) {
        if (!seeker.alive || seeker.reachedTarget) continue;

        int tIdx = -1;
        double bestDist = std::numeric_limits<double>::max();

        // Prefer an unclaimed target first; fall back to the nearest alive target
        // if every live target is already assigned to another seeker.
        for (int i = 0; i < static_cast<int>(m_targets.size()); ++i) {
            if (!m_targets[i].alive) continue;
            if (targetAssigned[i]) continue;

            double dr = seeker.row - m_targets[i].row;
            double dc = seeker.col - m_targets[i].col;
            double dist = std::sqrt(dr * dr + dc * dc);

            if (dist < bestDist) {
                bestDist = dist;
                tIdx = i;
            }
        }

        if (tIdx < 0) {
            for (int i = 0; i < static_cast<int>(m_targets.size()); ++i) {
                if (!m_targets[i].alive) continue;

                double dr = seeker.row - m_targets[i].row;
                double dc = seeker.col - m_targets[i].col;
                double dist = std::sqrt(dr * dr + dc * dc);

                if (dist < bestDist) {
                    bestDist = dist;
                    tIdx = i;
                }
            }
        }

        if (tIdx < 0) continue;

        targetAssigned[tIdx] = true;

        if (seeker.targetId != m_targets[tIdx].id || !seeker.hasPath()) {
            seeker.targetId = m_targets[tIdx].id;
            seeker.computePath(pf, m_targets[tIdx].row, m_targets[tIdx].col);

            if (seeker.path.empty()) {
                std::cout << "  Seeker " << seeker.id
                          << ": no path to target " << tIdx << "\n";
            }
        }
    }
}

void Simulation::assignHunters(const Pathfinding& pf) {
    for (auto& hunter : m_hunters) {
        if (!hunter.alive) continue;

        int bestIdx = -1;
        double bestDist = std::numeric_limits<double>::max();

        for (int i = 0; i < static_cast<int>(m_seekers.size()); ++i) {
            const auto& seeker = m_seekers[i];
            if (!seeker.alive || seeker.reachedTarget) continue;

            double dr = hunter.row - seeker.row;
            double dc = hunter.col - seeker.col;
            double dist = std::sqrt(dr * dr + dc * dc);

            if (dist < bestDist) {
                bestDist = dist;
                bestIdx = i;
            }
        }

        if (bestIdx < 0) continue;

        if (hunter.targetId != m_seekers[bestIdx].id || !hunter.hasPath()) {
            hunter.targetId = m_seekers[bestIdx].id;
            hunter.computePath(pf, m_seekers[bestIdx].row, m_seekers[bestIdx].col);

            if (hunter.path.empty()) {
                std::cout << "  Hunter " << hunter.id << ": no path to seeker " << bestIdx << "\n";
            }
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════════
//  BUILD RESULT
// ════════════════════════════════════════════════════════════════════════════════

SimResult Simulation::buildResult(int totalSteps) const {
    SimResult result;
    result.totalSteps = totalSteps;
    result.allTargetsDestroyed = true;
    result.allSeekersDead = true;
    result.maxNoiseLevel = m_maxNoiseLevel;

    // ── Seekers ──
    for (const auto& s : m_seekers) {
        SimResult::SeekerResult sr;
        sr.id = s.id;
        sr.stepsTaken = s.stepsTaken;
        sr.pathCost = s.pathCost;
        sr.nodesExpanded = s.nodesExpanded;
        sr.reachedTarget = s.reachedTarget;
        sr.targetId = s.targetId;
        sr.moveHistory = s.moveHistory;

        sr.detected = s.detected;
        sr.firstDetectedAtStep = s.firstDetectedAtStep;
        sr.firstDetectedByDetector = s.firstDetectedByDetector;

        sr.intercepted = s.intercepted;
        sr.interceptedByInterceptor = s.interceptedByInterceptor;
        sr.interceptedAtStep = s.interceptedAtStep;
        result.seekerResults.push_back(sr);

        if (s.alive) result.allSeekersDead = false;
    }

    // ── Hunters ──
    for (const auto& h : m_hunters) {
        SimResult::HunterResult hr;
        hr.id = h.id;
        hr.stepsTaken = h.stepsTaken;
        hr.pathCost = h.pathCost;
        hr.nodesExpanded = h.nodesExpanded;
        hr.moveHistory = h.moveHistory;
        hr.targetId = h.targetId;
        hr.capturedSeeker = h.capturedSeeker;
        hr.capturedSeekerId = h.capturedSeekerId;
        hr.capturedAtStep = h.capturedAtStep;
        result.hunterResults.push_back(hr);
    }

    // ── Targets ──
    for (const auto& t : m_targets) {
        SimResult::TargetResult tr;
        tr.id = t.id;
        tr.row = t.row;
        tr.col = t.col;
        tr.destroyed = !t.alive;
        tr.destroyedAtStep = -1;
        tr.destroyedBySeeker = -1;
        result.targetResults.push_back(tr);

        if (t.alive) result.allTargetsDestroyed = false;
    }

    // ── Detectors (now sensor-only) ──
    for (const auto& d : m_detectors) {
        SimResult::DetectorResult dr;
        dr.id = d.id;
        dr.row = d.row;
        dr.col = d.col;
        dr.sensingRadius = d.sensingRadius;
        dr.sightingCount = d.sightingCount;
        for (const auto& s : d.sightings) {
            dr.sightings.push_back({s.seekerId, s.step});
        }
        result.detectorResults.push_back(dr);
    }

    // ── Interceptors (new) ──
    for (const auto& i : m_interceptors) {
        SimResult::InterceptorResult ir;
        ir.id = i.id;
        ir.row = i.row;
        ir.col = i.col;
        ir.killRadius = i.killRadius;
        ir.killCount = i.killCount;
        for (const auto& ic : i.intercepts) {
            ir.intercepts.push_back({ic.seekerId, ic.step});
        }
        result.interceptorResults.push_back(ir);
    }

    result.computeSummary();
    return result;
}

// ════════════════════════════════════════════════════════════════════════════════
//  MAIN LOOP
// ════════════════════════════════════════════════════════════════════════════════

SimResult Simulation::run() {
    std::cout << "\n--- Simulation starting ---\n";

    Pathfinding pf(m_map.getGrid());
    assignTargets(pf);
    assignHunters(pf);

    int step = 0;
    bool allTargetsDead = false;
    bool allSeekersFinished = false;

    while (step < m_maxSteps && !allTargetsDead && !allSeekersFinished) {
        step++;

        // ── 1. Reassign hunters to the current seeker positions, then move them ──
        assignHunters(pf);

        for (auto& hunter : m_hunters) {
            if (!hunter.alive) continue;
            hunter.moveStep();
        }

        // Check hunter captures before seekers advance away
        for (auto& hunter : m_hunters) {
            if (!hunter.alive) continue;
            for (auto& seeker : m_seekers) {
                if (!seeker.alive || seeker.reachedTarget) continue;
                if (checkHunterCapture(hunter, seeker)) {
                    std::cout << "  Step " << step << ": Hunter " << hunter.id
                              << " captured Seeker " << seeker.id
                              << " at (" << seeker.row << "," << seeker.col << ")\n";
                    seeker.alive = false;
                    hunter.capturedSeeker = true;
                    hunter.capturedSeekerId = seeker.id;
                    hunter.capturedAtStep = step;
                    break;
                }
            }
        }

        // ── 2. Move seekers and apply noise ──
        for (auto& seeker : m_seekers) {
            if (!seeker.alive || seeker.reachedTarget) continue;
            seeker.moveStep();
        }

        // Recompute seeker targets after the seekers move so the hunter can chase their new position
        assignTargets(pf);

        if (m_maxNoiseLevel > 0.0) {
            for (auto& seeker : m_seekers) applyNoise(seeker);
        }

        // ── 3. SENSE: detectors update tracks ──
        updateDetectorTracks(step);

        // ── 4. SHOOT: interceptors engage tracked seekers ──
        checkInterceptorEngagements(step);

        // ── 5. Collisions: seekers vs. targets ──
        for (auto& seeker : m_seekers) {
            if (!seeker.alive || seeker.reachedTarget) continue;
            for (auto& target : m_targets) {
                if (!target.alive) continue;
                if (checkCollision(seeker, target)) {
                    std::cout << "  Step " << step << ": Seeker " << seeker.id
                              << " reached Target " << target.id
                              << " at (" << target.row << "," << target.col << ")\n";
                    target.alive = false;
                    seeker.reachedTarget = true;
                    break;
                }
            }
        }

        // ── 6. Retarget if needed ──
        bool needsRetarget = false;
        for (const auto& seeker : m_seekers) {
            if (!seeker.alive || seeker.reachedTarget) continue;
            if (seeker.targetId >= 0 &&
                seeker.targetId < static_cast<int>(m_targets.size())) {
                if (!m_targets[seeker.targetId].alive) {
                    needsRetarget = true; break;
                }
            }
            if (!seeker.hasPath()) { needsRetarget = true; break; }
        }
        if (needsRetarget) assignTargets(pf);
        bool needsHunterRetarget = false;
        for (const auto& hunter : m_hunters) {
            if (!hunter.alive) continue;
            if (hunter.targetId >= 0 && hunter.targetId < static_cast<int>(m_seekers.size())) {
                if (!m_seekers[hunter.targetId].alive || m_seekers[hunter.targetId].reachedTarget) {
                    needsHunterRetarget = true; break;
                }
            }
            if (!hunter.hasPath()) { needsHunterRetarget = true; break; }
        }
        if (needsHunterRetarget) assignHunters(pf);

        // ── 7. Termination ──
        allTargetsDead = true;
        for (const auto& t : m_targets) {
            if (t.alive) { allTargetsDead = false; break; }
        }
        allSeekersFinished = true;
        for (const auto& s : m_seekers) {
            if (s.alive && !s.reachedTarget && s.hasPath()) {
                allSeekersFinished = false; break;
            }
        }
        if (!allSeekersFinished) {
            bool huntersStillActive = false;
            for (const auto& h : m_hunters) {
                if (h.alive && h.hasPath()) { huntersStillActive = true; break; }
            }
            if (!huntersStillActive && m_hunters.empty()) {
                allSeekersFinished = true;
            }
        }
    }

    std::cout << "--- Simulation finished at step " << step << " ---\n";

    SimResult result = buildResult(step);

    // Patch in destruction info on targets
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