#include "simulation.h"
#include <cmath>
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
    // Build agent lists from the spawn config
    int seekerId = 0;
    int targetId = 0;
    int detectorId = 0;
    double detectorRadius = config.getDetectorRadius();

    for (const auto& unit : config.getUnits()) {
        if (unit.type == "seeker") {
            m_seekers.emplace_back(seekerId++, unit.row, unit.col);
        } else if (unit.type == "target") {
            m_targets.emplace_back(targetId++, unit.row, unit.col);
        } else if (unit.type == "detector") {
            m_detectors.emplace_back(detectorId++, unit.row, unit.col, detectorRadius);
        }
    }

    std::cout << "Simulation created: " << m_seekers.size() << " seekers, "
              << m_targets.size() << " targets, "
              << m_detectors.size() << " detectors (radius=" << detectorRadius << "), "
              << "noise=" << m_maxNoiseLevel << ", "
              << "max " << m_maxSteps << " steps\n";
}

// ════════════════════════════════════════════════════════════════════════════════
//  FIND NEAREST TARGET
// ════════════════════════════════════════════════════════════════════════════════

int Simulation::findNearestTarget(const SeekerAgent& seeker) const {
    int bestIdx = -1;
    double bestDist = std::numeric_limits<double>::max();

    for (int i = 0; i < static_cast<int>(m_targets.size()); i++) {
        if (!m_targets[i].alive) continue;

        // Euclidean distance for target selection (not pathfinding)
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
    // Seeker reaches target when it's on the same cell
    return seeker.row == target.row && seeker.col == target.col;
}

// ════════════════════════════════════════════════════════════════════════════════
//  DETECTOR INTERCEPTION
// ════════════════════════════════════════════════════════════════════════════════

void Simulation::checkDetectorIntercepts(int currentStep) {
    for (auto& seeker : m_seekers) {
        if (!seeker.alive || seeker.reachedTarget) continue;

        for (auto& detector : m_detectors) {
            if (!detector.alive) continue;

            if (detector.isInRange(seeker.row, seeker.col)) {
                std::cout << "  Step " << currentStep << ": Detector " << detector.id
                          << " intercepted Seeker " << seeker.id
                          << " at (" << seeker.row << "," << seeker.col << ")\n";

                // Kill the seeker
                seeker.alive = false;
                seeker.intercepted = true;
                seeker.interceptedByDetector = detector.id;
                seeker.interceptedAtStep = currentStep;

                // Log on the detector side
                detector.interceptCount++;
                detector.intercepts.push_back({seeker.id, currentStep});

                break;  // seeker is dead, no need to check more detectors
            }
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════════
//  APPLY NOISE
// ════════════════════════════════════════════════════════════════════════════════
//
//  After A* computes next position (x2, y2), the actual position becomes
//  (x2 + rx, y2 + ry) where rx, ry ∈ [-N, N] (uniform random integers).
//  This simulates environmental noise like waves and wind.
//
//  If the noisy position is out of bounds or blocked (land), the seeker
//  stays at the A* position — the noise is "absorbed" by the obstacle.
//  A line-of-sight check (Bresenham's line) ensures the displacement
//  doesn't cross any land cells, preventing seekers from teleporting
//  over land masses to disconnected water bodies.
//
//  After displacement, the seeker's pre-computed A* path is invalidated
//  and must be recomputed from the new position.
//
// ════════════════════════════════════════════════════════════════════════════════

bool Simulation::applyNoise(SeekerAgent& seeker) {
    if (m_maxNoiseLevel <= 0) return false;
    if (!seeker.alive || seeker.reachedTarget) return false;

    // Generate random displacement in [-N, N]
    std::uniform_int_distribution<int> dist(-m_maxNoiseLevel, m_maxNoiseLevel);
    int rx = dist(m_rng);
    int ry = dist(m_rng);

    if (rx == 0 && ry == 0) return false;  // no displacement

    int newRow = seeker.row + ry;
    int newCol = seeker.col + rx;

    // Check if noisy position is valid and passable
    if (!m_map.isValid(newRow, newCol)) return false;
    if (!m_map.isPassable(newRow, newCol)) return false;

    // ── Line-of-sight check ──
    // Walk from current position to noisy position using Bresenham's line.
    // Reject the displacement if ANY intermediate cell is blocked (land).
    // This prevents seekers from "teleporting" over land masses.
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

            // Check every cell along the line (except start, which we're on)
            if (!m_map.isValid(r, c) || !m_map.isPassable(r, c)) {
                return false;  // path crosses land — reject displacement
            }
        }
    }

    // Apply the displacement
    seeker.row = newRow;
    seeker.col = newCol;

    // Record the noisy position in move history
    seeker.moveHistory.push_back({newRow, newCol});

    // Invalidate current path — must be recomputed from new position
    seeker.path.clear();
    seeker.pathIndex = 0;

    return true;
}

// ════════════════════════════════════════════════════════════════════════════════
//  ASSIGN TARGETS
// ════════════════════════════════════════════════════════════════════════════════

void Simulation::assignTargets(const Pathfinding& pf) {
    for (auto& seeker : m_seekers) {
        if (!seeker.alive || seeker.reachedTarget) continue;

        int tIdx = findNearestTarget(seeker);
        if (tIdx < 0) continue;  // no alive targets

        // Only recompute path if we need a new target or have no path
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

    // Seeker results
    for (const auto& s : m_seekers) {
        SimResult::SeekerResult sr;
        sr.id = s.id;
        sr.stepsTaken = s.stepsTaken;
        sr.pathCost = s.pathCost;
        sr.nodesExpanded = s.nodesExpanded;
        sr.reachedTarget = s.reachedTarget;
        sr.targetId = s.targetId;
        sr.moveHistory = s.moveHistory;
        sr.intercepted = s.intercepted;
        sr.interceptedByDetector = s.interceptedByDetector;
        sr.interceptedAtStep = s.interceptedAtStep;
        result.seekerResults.push_back(sr);

        if (s.alive) result.allSeekersDead = false;
    }

    // Target results
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

    // Detector results
    for (const auto& d : m_detectors) {
        SimResult::DetectorResult dr;
        dr.id = d.id;
        dr.row = d.row;
        dr.col = d.col;
        dr.radius = d.radius;
        dr.interceptCount = d.interceptCount;
        for (const auto& ic : d.intercepts) {
            dr.intercepts.push_back({ic.seekerId, ic.step});
        }
        result.detectorResults.push_back(dr);
    }

    result.computeSummary();
    return result;
}

// ════════════════════════════════════════════════════════════════════════════════
//  RUN SIMULATION
// ════════════════════════════════════════════════════════════════════════════════

SimResult Simulation::run() {
    std::cout << "\n--- Simulation starting ---\n";

    // Create pathfinder from the current grid
    Pathfinding pf(m_map.getGrid());

    // Initial target assignment — compute paths for all seekers
    assignTargets(pf);

    int step = 0;
    bool allTargetsDead = false;
    bool allSeekersFinished = false;

    while (step < m_maxSteps && !allTargetsDead && !allSeekersFinished) {
        step++;

        // ── Move each seeker one step ──
        for (auto& seeker : m_seekers) {
            if (!seeker.alive || seeker.reachedTarget) continue;
            seeker.moveStep();
        }

        // ── Apply environmental noise (wave/wind displacement) ──
        // After A* gives the next position, displace by random (rx, ry).
        // This invalidates the pre-computed path, forcing recomputation.
        if (m_maxNoiseLevel > 0) {
            for (auto& seeker : m_seekers) {
                applyNoise(seeker);
            }
        }

        // ── Check detector interceptions FIRST ──
        // Seekers that enter a detector's radius are destroyed
        // before they can reach a target on the same step
        checkDetectorIntercepts(step);

        // ── Check collisions: did any seeker reach its target? ──
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

        // ── Check if we need to retarget ──
        bool needsRetarget = false;
        for (const auto& seeker : m_seekers) {
            if (!seeker.alive || seeker.reachedTarget) continue;
            // Check if our target is still alive
            if (seeker.targetId >= 0 && seeker.targetId < static_cast<int>(m_targets.size())) {
                if (!m_targets[seeker.targetId].alive) {
                    needsRetarget = true;
                    break;
                }
            }
            // Check if we ran out of path
            if (!seeker.hasPath()) {
                needsRetarget = true;
                break;
            }
        }
        if (needsRetarget) {
            assignTargets(pf);
        }

        // ── Check termination conditions ──
        allTargetsDead = true;
        for (const auto& t : m_targets) {
            if (t.alive) { allTargetsDead = false; break; }
        }

        allSeekersFinished = true;
        for (const auto& s : m_seekers) {
            if (s.alive && !s.reachedTarget && s.hasPath()) {
                allSeekersFinished = false;
                break;
            }
        }
    }

    std::cout << "--- Simulation finished at step " << step << " ---\n";

    // Build and return results
    SimResult result = buildResult(step);

    // Patch in destruction step info
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