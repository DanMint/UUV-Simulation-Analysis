#ifndef SIMULATION_H
#define SIMULATION_H

#include <memory>
#include <random>
#include <string>
#include <vector>

#include "mapCreation.h"
#include "pathfinding.h"

#include "targetAgent.h"

#include "seekerAgent.h"
#include "basicSeekerAgent.h"
#include "fastSeekerAgent.h"
#include "evaderSeekerAgent.h"

#include "detectorAgent.h"

#include "interceptorAgent.h"
#include "basicInterceptorAgent.h"
#include "mediumInterceptorAgent.h"
#include "advancedInterceptorAgent.h"

#include "spawnConfig.h"
#include "simResult.h"

/**
 * Simulation
 *
 * Runs one complete simulation. Per step:
 *   1. Each living seeker performs its concrete movement behavior
 *   2. Environmental noise displaces seekers and may invalidate paths
 *   3. Detectors update tracks
 *   4. Interceptors engage tracked seekers
 *   5. Check seeker -> target collisions
 *   6. If any target was destroyed, surviving seekers retarget
 *   7. Continue until termination or maxSteps
 *
 * Unit model:
 *   - SpawnConfig supplies a broad category and a concrete type.
 *   - category selects the agent family.
 *   - type selects the concrete C++ implementation.
 *
 * Current seeker mappings:
 *   category="seeker", type="basic"  -> BasicSeekerAgent
 *   category="seeker", type="fast"   -> FastSeekerAgent
 *   category="seeker", type="evader" -> EvaderSeekerAgent
 *
 * Current interceptor mappings:
 *   category="interceptor", type="basic"    -> BasicInterceptorAgent
 *   category="interceptor", type="medium"   -> MediumInterceptorAgent
 *   category="interceptor", type="advanced" -> AdvancedInterceptorAgent
 *
 * Doctrine: SENSE-THEN-SHOOT.
 *   - A lone detector sees but cannot kill.
 *   - A lone interceptor cannot fire without a detector track.
 */
class Simulation {
public:
    Simulation(
        MapCreation& map,
        const SpawnConfig& config,
        int maxSteps = 2000
    );

    /** Run to completion and return the final result. */
    SimResult run();

private:
    MapCreation& m_map;
    int m_maxSteps;
    double m_maxNoiseLevel;

    /**
     * Seekers require polymorphic storage because different concrete seeker
     * types derive from SeekerAgent.
     *
     * unique_ptr prevents object slicing and gives Simulation sole ownership
     * of every seeker object.
     */
    std::vector<std::unique_ptr<SeekerAgent>> m_seekers;

    // The current SeekerAgent hierarchy does not expose getType(), so the
    // SpawnConfig type is kept parallel to m_seekers for result output.
    std::vector<std::string> m_seekerTypes;

    /*
     * These categories currently have only their existing concrete classes.
     * They may be converted to polymorphic unique_ptr collections later using
     * the same pattern as m_seekers.
     */
    std::vector<TargetAgent>   m_targets;
    std::vector<DetectorAgent> m_detectors;

    // Polymorphic interceptor collection.
    std::vector<std::unique_ptr<InterceptorAgent>> m_interceptors;

    /*
     * Type tracking retained for result serialization. Vector indices
     * correspond to their associated agent vectors.
     */
    std::vector<std::string> m_targetTypes;
    std::vector<std::string> m_detectorTypes;
    std::vector<std::string> m_interceptorTypes;

    mutable std::mt19937 m_rng;

    /** Nearest living target to a seeker (Euclidean). -1 if none. */
    int findNearestTarget(const SeekerAgent& seeker) const;

    /** True if seeker and target occupy the same cell. */
    bool checkCollision(
        const SeekerAgent& seeker,
        const TargetAgent& target
    ) const;

    /** Detectors look for seekers and update sticky tracks. */
    void updateDetectorTracks(int currentStep);

    /** Interceptors engage tracked seekers in range. */
    void checkInterceptorEngagements(int currentStep);

    /** Apply environmental noise and invalidate the path on displacement. */
    bool applyNoise(SeekerAgent& seeker);

    /** Reassign each seeker to a living target and recompute paths. */
    void assignTargets(const Pathfinding& pathfinding);

    /** Build the final result structure from the current agent state. */
    SimResult buildResult(int totalSteps) const;
};

#endif // SIMULATION_H