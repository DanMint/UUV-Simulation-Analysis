#include "simulation.h"
#include <cmath>
#include <limits>
#include <iostream>

// ════════════════════════════════════════════════════════════════════════════════
//  CONSTRUCTOR
// ════════════════════════════════════════════════════════════════════════════════

Simulation::Simulation(MapCreation& map, const SpawnConfig& config, int maxSteps)
    : m_map(map), m_maxSteps(maxSteps)
{
    // Build agent lists from the spawn config
    int seekerId = 0;
    int targetId = 0;

    for (const auto& unit : config.getUnits()) {
        if (unit.type == "seeker") {
            m_seekers.emplace_back(seekerId++, unit.row, unit.col);
        } else if (unit.type == "target") {
            m_targets.emplace_back(targetId++, unit.row, unit.col);
        }
    }

    std::cout << "Simulation created: " << m_seekers.size() << " seekers, "
              << m_targets.size() << " targets, max " << m_maxSteps << " steps\n";
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

                    // Store destruction info (find the target result later)
                    // We track it directly on the target for now
                    break;
                }
            }
        }

        // ── Check if we need to retarget ──
        // If a target was just destroyed, remaining seekers may need new paths
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
            // Find which seeker destroyed this target
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