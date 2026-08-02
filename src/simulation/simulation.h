#ifndef SIMULATION_H
#define SIMULATION_H

#include <vector>
#include <string>
#include <random>
#include "mapCreation.h"
#include "pathfinding.h"
#include "targetAgent.h"
#include "seekerAgent.h"
#include "detectorAgent.h"
#include "interceptorAgent.h"
#include "spawnConfig.h"
#include "simResult.h"
#include "attackerAgent.h"

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
 * Supports both batch (run()) and step-by-step (stepOnce()) execution
 * modes. Step-by-step is used by SimulationVisualizer for live animation.
 *
 * Doctrine: SENSE-THEN-SHOOT.
 *   - A lone detector sees but cannot kill.
 *   - A lone interceptor cannot fire - no tracks, no shots.
 *
 * Produces a SimResult when finished. Does NOT save the result file -
 * that is SimResult's responsibility.
 */
class Simulation {
public:
    /**
     * @param map       Map reference (must outlive the simulation)
     * @param config    Scenario configuration
     * @param maxSteps  Step limit before forced termination
     * @param seed      RNG seed for reproducible runs (default: random_device)
     */
    Simulation(MapCreation& map, const SpawnConfig& config, int maxSteps = 2000,
               unsigned seed = std::random_device{}());
    ~Simulation() { delete m_pf; }

    /** Retrieve the RNG seed used for this run (for reproducibility/replay). */
    unsigned getSeed() const { return m_seed; }

    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;

    /** Run to completion from a clean state. Re-allocates m_pf every call. */
    SimResult run();

    /**
     * Run to completion from current state, reusing existing m_pf if one exists.
     * Does NOT reset m_step or m_finished. Safely callable multiple times
     * without leaking m_pf. Used internally by finishFromCurrentState().
     */
    SimResult runFromCurrentState();

    /**
     * Continue stepping from current state until finished (no reset).
     * Uses existing m_pf, does NOT reset m_step or m_finished,
     * does NOT call assignTargets() again.
     * Safe to call mid-visualization as "skip to end".
     */
    void finishFromCurrentState();

    // -- Step-by-step API (used by SimulationVisualizer) --

    /** Advance one simulation step. Returns false if simulation ended. */
    bool stepOnce();

    /** True if simulation has finished. */
    bool isFinished() const { return m_finished; }

    /** Current step number (0-based). */
    int getStep() const { return m_step; }

    int getMaxSteps() const { return m_maxSteps; }

    /** Build the final result struct from current agent state. Public for visualizer. */
    SimResult buildResult(int totalSteps) const;

    // Const accessors for visualizer
    const std::vector<SeekerAgent>&      getSeekers()      const { return m_seekers; }
    const std::vector<TargetAgent>&      getTargets()      const { return m_targets; }
    const std::vector<DetectorAgent>&    getDetectors()    const { return m_detectors; }
    const std::vector<InterceptorAgent>& getInterceptors()  const { return m_interceptors; }
    const std::vector<AttackerAgent>&    getAttackers()    const { return m_attackers; }

private:
    MapCreation& m_map;
    int m_maxSteps;
    double m_maxNoiseLevel;
    bool m_finished;
    int  m_step;

    std::vector<SeekerAgent>      m_seekers;
    std::vector<TargetAgent>      m_targets;
    std::vector<DetectorAgent>    m_detectors;
    std::vector<InterceptorAgent> m_interceptors;
    std::vector<AttackerAgent>     m_attackers;

    unsigned m_seed;            ///< RNG seed used for this run (reproducible)
    mutable std::mt19937 m_rng;

    Pathfinding* m_pf;  // persistent pathfinder (reused across steps)

    /** Nearest living target to a seeker (Euclidean). -1 if none. */
    int findNearestTarget(const SeekerAgent& seeker) const;

    /** True if seeker and target occupy the same cell. */
    bool checkCollision(const SeekerAgent& seeker, const TargetAgent& target) const;

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

    /** Templated noise implementation (works with SeekerAgent and AttackerAgent). */
    template <typename Agent>
    bool applyNoiseImpl(Agent& agent);

    /** Re-assign each seeker to its nearest target and recompute paths. */
    void assignTargets(const Pathfinding& pf);

    void updateAttackerStates(int currentStep, const Pathfinding& pf);

    // Internal: one step of the main loop, used by both run() and stepOnce()
    void executeOneStepInternal(int step, bool verbose);
};

#endif // SIMULATION_H
