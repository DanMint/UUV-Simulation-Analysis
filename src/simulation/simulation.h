#ifndef SIMULATION_H
#define SIMULATION_H

#include <vector>
#include <string>
#include <random>
#include "mapCreation.h"
#include "pathfinding.h"
#include "agent.h"
#include "spawnConfig.h"
#include "simResult.h"

/**
 * Simulation
 *
 * Runs one complete simulation:
 *   1. Seekers compute paths to nearest targets (A*)
 *   2. Each step, seekers move one cell along their path
 *   3. Apply environmental noise (wave/wind displacement)
 *   4. Check if any seeker is within a detector's radius (intercepted → destroyed)
 *   5. Check if any seeker reached a target (collision → target destroyed)
 *   6. If target destroyed, remaining seekers retarget
 *   7. Stop when all targets destroyed, all seekers dead, or max steps reached
 *
 * Produces a SimResult when finished.
 * Does NOT save results — that's SimResult's job.
 */
class Simulation {
public:
    /**
     * @param map       The grid (with units stamped on it)
     * @param config    Spawn configuration (unit positions + detector radius + noise)
     * @param maxSteps  Maximum steps before forced termination
     */
    Simulation(MapCreation& map, const SpawnConfig& config, int maxSteps = 2000);

    /** Run the simulation to completion. Returns results. */
    SimResult run();

private:
    MapCreation& m_map;
    int m_maxSteps;
    int m_maxNoiseLevel;

    std::vector<SeekerAgent> m_seekers;
    std::vector<TargetAgent> m_targets;
    std::vector<DetectorAgent> m_detectors;

    // Random number generator for noise
    mutable std::mt19937 m_rng;

    /** Find the nearest alive target to a seeker. Returns target index or -1. */
    int findNearestTarget(const SeekerAgent& seeker) const;

    /** Check if a seeker has reached its target (same cell). */
    bool checkCollision(const SeekerAgent& seeker, const TargetAgent& target) const;

    /** Check all detectors against all alive seekers. Kill seekers in range. */
    void checkDetectorIntercepts(int currentStep);

    /**
     * Apply environmental noise to a seeker's position.
     * Displaces by (rx, ry) where rx, ry ∈ [-N, N].
     * If the noisy position is invalid or blocked, seeker stays put.
     * Returns true if position was displaced.
     */
    bool applyNoise(SeekerAgent& seeker);

    /** Assign each seeker to its nearest target and compute paths. */
    void assignTargets(const Pathfinding& pf);

    /** Build the final result struct from current agent state. */
    SimResult buildResult(int totalSteps) const;
};

#endif // SIMULATION_H