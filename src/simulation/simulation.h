#ifndef SIMULATION_H
#define SIMULATION_H

#include <vector>
#include <string>
#include <utility>
#include "mapCreation.h"
#include "pathfinding.h"
#include "agent.h"
#include "spawnConfig.h"

/**
 * SimResult
 *
 * Holds all the data from one complete simulation run.
 * Used for analysis after the run is finished.
 */
struct SimResult {
    // Run info
    int totalSteps;          // how many simulation steps ran
    bool allTargetsDestroyed;
    bool allSeekersDead;

    // Per-seeker data
    struct SeekerResult {
        int id;
        int stepsTaken;
        double pathCost;
        int nodesExpanded;
        bool reachedTarget;
        int targetId;            // which target it was chasing
        std::vector<Pathfinding::Pos> moveHistory;  // full path walked
    };
    std::vector<SeekerResult> seekerResults;

    // Per-target data
    struct TargetResult {
        int id;
        int row;
        int col;
        bool destroyed;
        int destroyedAtStep;     // -1 if survived
        int destroyedBySeeker;   // -1 if survived
    };
    std::vector<TargetResult> targetResults;

    // Summary
    int targetsDestroyed;
    int seekersThatReached;
    double avgStepsToTarget;

    void computeSummary() {
        targetsDestroyed = 0;
        seekersThatReached = 0;
        double totalStepsReached = 0;

        for (const auto& t : targetResults) {
            if (t.destroyed) targetsDestroyed++;
        }
        for (const auto& s : seekerResults) {
            if (s.reachedTarget) {
                seekersThatReached++;
                totalStepsReached += s.stepsTaken;
            }
        }
        avgStepsToTarget = (seekersThatReached > 0)
            ? totalStepsReached / seekersThatReached
            : 0.0;
    }

    void print() const {
        std::cout << "\n=== Simulation Result ===" << std::endl;
        std::cout << "  Total steps:        " << totalSteps << std::endl;
        std::cout << "  Targets destroyed:  " << targetsDestroyed
                  << " / " << targetResults.size() << std::endl;
        std::cout << "  Seekers reached:    " << seekersThatReached
                  << " / " << seekerResults.size() << std::endl;
        std::cout << "  Avg steps to target: " << avgStepsToTarget << std::endl;

        std::cout << "\n  Seekers:" << std::endl;
        for (const auto& s : seekerResults) {
            std::cout << "    Seeker " << s.id << ": "
                      << s.stepsTaken << " steps, "
                      << s.nodesExpanded << " nodes expanded, "
                      << "path cost " << s.pathCost;
            if (s.reachedTarget)
                std::cout << " → reached target " << s.targetId;
            else
                std::cout << " → did NOT reach target";
            std::cout << std::endl;
        }

        std::cout << "\n  Targets:" << std::endl;
        for (const auto& t : targetResults) {
            std::cout << "    Target " << t.id << " at (" << t.row << "," << t.col << "): ";
            if (t.destroyed)
                std::cout << "DESTROYED at step " << t.destroyedAtStep
                          << " by seeker " << t.destroyedBySeeker;
            else
                std::cout << "survived";
            std::cout << std::endl;
        }
        std::cout << "=========================\n" << std::endl;
    }
};

/**
 * Simulation
 *
 * Runs one complete simulation:
 *   1. Seekers compute paths to nearest targets (A*)
 *   2. Each step, seekers move one cell along their path
 *   3. Check if any seeker reached a target (collision)
 *   4. If target destroyed, remaining seekers retarget
 *   5. Stop when all targets destroyed or max steps reached
 */
class Simulation {
public:
    /**
     * @param map       The grid (with units stamped on it)
     * @param config    Spawn configuration (unit positions)
     * @param maxSteps  Maximum steps before forced termination
     */
    Simulation(MapCreation& map, const SpawnConfig& config, int maxSteps = 2000);

    /** Run the simulation to completion. Returns results. */
    SimResult run();

private:
    MapCreation& m_map;
    int m_maxSteps;

    std::vector<SeekerAgent> m_seekers;
    std::vector<TargetAgent> m_targets;

    /** Find the nearest alive target to a seeker. Returns target index or -1. */
    int findNearestTarget(const SeekerAgent& seeker) const;

    /** Check if a seeker has reached its target (within 1 cell). */
    bool checkCollision(const SeekerAgent& seeker, const TargetAgent& target) const;

    /** Assign each seeker to its nearest target and compute paths. */
    void assignTargets(const Pathfinding& pf);

    /** Build the final result struct from current state. */
    SimResult buildResult(int totalSteps) const;
};

#endif // SIMULATION_H