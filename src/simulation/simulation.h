#ifndef SIMULATION_H
#define SIMULATION_H

#include <random>
#include <string>
#include <vector>

#include "mapCreation.h"
#include "pathfinding.h"
#include "targetAgent.h"
#include "seekerAgent.h"
#include "detectorAgent.h"
#include "interceptorAgent.h"
#include "spawnConfig.h"
#include "simResult.h"

/**
 * Simulation
 *
 * Runs one complete simulation. Per step:
 *   1. Each living seeker moves one cell along its A* path
 *   2. Environmental noise displaces seekers and may invalidate paths
 *   3. Detectors update tracks
 *   4. Interceptors engage tracked seekers
 *   5. Check seeker -> target collisions
 *   6. If any target was destroyed, surviving seekers retarget
 *   7. Continue until termination or maxSteps
 *
 * Unit model:
 *   - SpawnConfig supplies a broad category and a concrete type.
 *   - category selects the agent family: seeker, target, detector, interceptor.
 *   - type selects the implementation within that family.
 *   - Currently, "basic" is the only supported concrete type.
 *
 * Doctrine: SENSE-THEN-SHOOT.
 *   - A lone detector sees but cannot kill.
 *   - A lone interceptor cannot fire without a detector track.
 */
class Simulation {
public:
    Simulation(MapCreation& map, const SpawnConfig& config, int maxSteps = 2000);

    /** Run to completion and return the final result. */
    SimResult run();

private:
    MapCreation& m_map;
    int m_maxSteps;
    double m_maxNoiseLevel;

    std::vector<SeekerAgent>      m_seekers;
    std::vector<TargetAgent>      m_targets;
    std::vector<DetectorAgent>    m_detectors;
    std::vector<InterceptorAgent> m_interceptors;

    // Concrete types are stored parallel to the agent vectors because the
    // existing agent classes do not yet contain a type field.
    std::vector<std::string> m_seekerTypes;
    std::vector<std::string> m_targetTypes;
    std::vector<std::string> m_detectorTypes;
    std::vector<std::string> m_interceptorTypes;

    mutable std::mt19937 m_rng;

    /** Nearest living target to a seeker (Euclidean). -1 if none. */
    int findNearestTarget(const SeekerAgent& seeker) const;

    /** True if seeker and target occupy the same cell. */
    bool checkCollision(const SeekerAgent& seeker, const TargetAgent& target) const;

    /** Detectors look for seekers and update sticky tracks. */
    void updateDetectorTracks(int currentStep);

    /** Interceptors engage tracked seekers in range. */
    void checkInterceptorEngagements(int currentStep);

    /** Apply environmental noise and invalidate the path on displacement. */
    bool applyNoise(SeekerAgent& seeker);

    /** Reassign each seeker to a living target and recompute paths. */
    void assignTargets(const Pathfinding& pf);

    /** Build the final result structure from the current agent state. */
    SimResult buildResult(int totalSteps) const;
};

#endif // SIMULATION_H