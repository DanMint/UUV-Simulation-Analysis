#ifndef SIMULATION_H
#define SIMULATION_H

#include <vector>
#include <string>
#include <random>
#include "mapCreation.h"
#include "pathfinding.h"
#include "targetAgent.h"
#include "seekerAgent.h"
#include "hunterAgent.h"
#include "detectorAgent.h"
#include "interceptorAgent.h"
#include "spawnConfig.h"
#include "simResult.h"

/**
 * Simulation
 *
 * Runs one complete simulation. Per step:
 *   1. Each living seeker moves one cell along its A* path
 *   2. Environmental noise (wave/wind) displaces seekers and may
 *      invalidate paths
 *   3. Detectors update tracks: any seeker inside a detector's sensing
 *      radius becomes `detected` (sticky)
 *   4. Interceptors engage: any detected seeker inside an interceptor's
 *      kill radius is rolled against the interceptor's probabilistic
 *      kill model
 *   5. Check seeker -> target collisions
 *   6. If any target was destroyed, surviving seekers retarget
 *   7. Loop until all targets dead, all seekers dead/reached, or
 *      maxSteps reached
 *
 * Doctrine: SENSE-THEN-SHOOT.
 *   - A lone detector sees but cannot kill.
 *   - A lone interceptor cannot fire — no tracks, no shots.
 *
 * Produces a SimResult when finished. Does NOT save the result file —
 * that is SimResult's responsibility.
 */
class Simulation {
public:
    Simulation(MapCreation& map, const SpawnConfig& config, int maxSteps = 2000);

    /** Run to completion. Returns results. */
    SimResult run();

private:
    MapCreation& m_map;
    int m_maxSteps;
    double m_maxNoiseLevel;

    std::vector<SeekerAgent>      m_seekers;
    std::vector<HunterAgent>      m_hunters;
    std::vector<TargetAgent>      m_targets;
    std::vector<DetectorAgent>    m_detectors;
    std::vector<InterceptorAgent> m_interceptors;

    mutable std::mt19937 m_rng;

    /** Nearest living target to a seeker (Euclidean). -1 if none. */
    int findNearestTarget(const SeekerAgent& seeker) const;

    /** True if seeker and target occupy the same cell. */
    bool checkCollision(const SeekerAgent& seeker, const TargetAgent& target) const;

    /** True if hunter and seeker occupy the same cell. */
    bool checkHunterCapture(const HunterAgent& hunter, const SeekerAgent& seeker) const;

    /**
     * Detectors look for seekers; sets `detected = true` on first sight
     * and logs every sighting (including repeats).
     */
    void updateDetectorTracks(int currentStep);

    /**
     * Interceptors engage tracked seekers in range using their
     * probabilistic kill model. Untracked seekers are skipped.
     */
    void checkInterceptorEngagements(int currentStep);

    /**
     * Apply environmental noise to a seeker's position. Bresenham
     * line-of-sight enforced so the seeker cannot jump over land.
     * Invalidates the seeker's path on success.
     */
    bool applyNoise(SeekerAgent& seeker);

    /** Re-assign each seeker to its nearest target and recompute paths. */
    void assignTargets(const Pathfinding& pf);

    /** Re-assign each hunter to its nearest alive seeker and recompute paths. */
    void assignHunters(const Pathfinding& pf);

    /** Build the final result struct from current agent state. */
    SimResult buildResult(int totalSteps) const;
};

#endif // SIMULATION_H