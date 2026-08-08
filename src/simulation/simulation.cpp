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
    int seekerId = 0, targetId = 0, detectorId = 0, interceptorId = 0, patrolId = 0; // patrolID =0 reason being constructor and its how we create sofar 
    double detRadius = config.getDetectorRadius();
    double intRadius = config.getInterceptorRadius();

    for (const auto& unit : config.getUnits()) {
        if (unit.type == "seeker") {
            m_seekers.emplace_back(seekerId++, unit.row, unit.col);
        } else if (unit.type == "target") {
            m_targets.emplace_back(targetId++, unit.row, unit.col);
        } else if (unit.type == "detector") {
            m_detectors.emplace_back(detectorId++, unit.row, unit.col, detRadius);
        } else if (unit.type == "interceptor") {
            m_interceptors.emplace_back(interceptorId++, unit.row, unit.col, intRadius);
        }
        else if (unit.type == "patrol_defender") {
            PatrolDefenderAgent p(patrolId++, unit.row, unit.col, detRadius, intRadius, true, 1.0f); //emplace means to create directly inside vecotr cannot modify vs p which is just a temp varibale to store it sicne i need to create add then store 
            // if this unit has a paired waypoint, add both points as the patrol route
            if (unit.waypointRow != -1) {
                p.addWaypoint(unit.row, unit.col);                  // point A
                p.addWaypoint(unit.waypointRow, unit.waypointCol);  // point B
                p.isDynamic = true;
            }
            m_patrolDefenders.push_back(p);
        }
    }

    std::cout << "Simulation created: "
              << m_seekers.size()      << " seekers, "
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
        // patrol defenders also sense --- but patrol has alot more going on with it 
        for (auto& patrol : m_patrolDefenders) {
            if (!patrol.alive) continue;
            if (!patrol.isInSensingRange(seeker.row, seeker.col)) continue;
            patrol.recordSighting(seeker.id, currentStep);
            if (!seeker.detected) {
                seeker.detected = true;
                seeker.firstDetectedAtStep = currentStep;
                seeker.firstDetectedByDetector = patrol.id;
                std::cout << "  Step " << currentStep
                          << ": Patrol Defender " << patrol.id
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
        // patrol defenders also shoot/kill probability --- 
        if (!seeker.alive) continue; // already killed above, skip
        for (auto& patrol : m_patrolDefenders) {
            if (!patrol.alive) continue;
            if (!patrol.isInKillRange(seeker.row, seeker.col)) continue;

            double pKill = patrol.killProbability(seeker.row, seeker.col);
            double r = roll(m_rng);
            double dr = patrol.row - seeker.row;
            double dc = patrol.col - seeker.col;
            double dist = std::sqrt(dr * dr + dc * dc);
            double ratio = (patrol.killRadius > 0.0) ? dist / patrol.killRadius : 0.0;

            if (r < pKill) {
                std::cout << "  Step " << currentStep
                          << ": Patrol Defender " << patrol.id
                          << " killed Seeker " << seeker.id
                          << " at (" << seeker.row << "," << seeker.col << ")"
                          << " [dist=" << std::fixed << std::setprecision(1)
                          << (ratio * 100) << "%, p=" << (pKill * 100) << "%]\n";
                seeker.alive = false;
                seeker.intercepted = true;
                seeker.interceptedByInterceptor = patrol.id;
                seeker.interceptedAtStep = currentStep;
                patrol.recordIntercept(seeker.id, currentStep);
                break;
            } else {
                std::cout << "  Step " << currentStep
                          << ": Patrol Defender " << patrol.id
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
    for (auto& seeker : m_seekers) {
        if (!seeker.alive || seeker.reachedTarget) continue;

        int tIdx = findNearestTarget(seeker);
        if (tIdx < 0) continue;

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

    // ── Patrol Defenders --- purpose is so when i do run code to packages this into teh simresults which inevitably saves to the json file in runs/
    // this gives patrol defenders their own section in the JSON output
    for (const auto& p : m_patrolDefenders) {
        SimResult::PatrolDefenderResult pr;
        pr.id = p.id;
        pr.row = p.row;
        pr.col = p.col;
        pr.isDynamic = p.isDynamic;        // was it moving or static this run
        pr.sensingRadius = p.sensingRadius; // sensing side
        pr.sightingCount = p.sightingCount;
        for (const auto& s : p.sightings) {
            pr.sightings.push_back({s.seekerId, s.step});
        }
        pr.killRadius = p.killRadius;       // killing side
        pr.killCount = p.killCount;
        for (const auto& ic : p.intercepts) {
            pr.intercepts.push_back({ic.seekerId, ic.step});
        }

        pr.waypoints = p.waypoints;
        pr.moveHistory = p.moveHistory;

        result.patrolDefenderResults.push_back(pr);
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

    int step = 0;
    bool allTargetsDead = false;
    bool allSeekersFinished = false;

    while (step < m_maxSteps && !allTargetsDead && !allSeekersFinished) {
        step++;

        // ── 1. Move ──
        for (auto& seeker : m_seekers) {
            if (!seeker.alive || seeker.reachedTarget) continue;
            seeker.moveStep();
        }

        // ── 1b. Move patrol defenders (dynamic only) ── this is the big every tick we keep calling the function to move beofre the sense 
        for (auto& patrol : m_patrolDefenders) {
            if (!patrol.alive) continue;
            if (patrol.isDynamic) patrol.moveTowardWaypoint();
        }

        // ── 2. Noise ──
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