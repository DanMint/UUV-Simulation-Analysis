#ifndef SIMRESULT_H
#define SIMRESULT_H

#include <vector>
#include <string>
#include <utility>
#include <iostream>
#include <fstream>
#include <stdexcept>

/**
 * SimResult
 *
 * Holds all output data from one simulation run. Responsible for:
 *   - Storing per-seeker, per-target, per-detector, per-interceptor results
 *   - Computing summary statistics
 *   - Printing results to console
 *   - Saving results to JSON
 *
 * Does NOT run the simulation (that is Simulation's job) and does NOT
 * own map/spawn data (that is SpawnConfig's job).
 */
struct SimResult {

    // ─── Per-seeker output ──────────────────────────────────────────

    struct SeekerResult {
        int id;
        int stepsTaken;
        double pathCost;
        int nodesExpanded;
        bool reachedTarget;
        int targetId;
        std::vector<std::pair<int,int>> moveHistory;

        // Detection (set by detectors, sense-then-shoot doctrine)
        bool detected;
        int firstDetectedAtStep;
        int firstDetectedByDetector;

        // Interception (set by interceptors)
        bool intercepted;
        int interceptedByInterceptor;
        int interceptedAtStep;
    };

    // ─── Per-target output ──────────────────────────────────────────

    struct TargetResult {
        int id;
        int row;
        int col;
        bool destroyed;
        int destroyedAtStep;
        int destroyedBySeeker;
    };

    // ─── Per-detector output (sense-only) ───────────────────────────

    struct DetectorResult {
        int id;
        int row;
        int col;
        double sensingRadius;
        int sightingCount;

        struct Sighting {
            int seekerId;
            int step;
        };
        std::vector<Sighting> sightings;
    };

    // ─── Per-interceptor output (kill-only) ─────────────────────────

    struct InterceptorResult {
        int id;
        int row;
        int col;
        double killRadius;
        int killCount;

        struct Intercept {
            int seekerId;
            int step;
        };
        std::vector<Intercept> intercepts;
    };

    // ─── Per-patrol-defender output  ────────────────────
    // Patrol defenders both sense AND kill, so this result combines
    // fields from both DetectorResult and InterceptorResult.
    // isDynamic tells us if it was moving or sitting still this run.

    struct PatrolDefenderResult {
        int id;
        int row, col;
        bool isDynamic;

        // sensing side
        double sensingRadius;
        int sightingCount;
        struct Sighting { int seekerId; int step; };
        std::vector<Sighting> sightings;

        // killing side
        double killRadius;
        int killCount;
        struct Intercept { int seekerId; int step; };
        std::vector<Intercept> intercepts;

        // Movement --- used this section to track the history so when we get to json/pngs we can pass along information 
        std::vector<std::pair<int,int>> waypoints;
        std::vector<std::pair<int,int>> moveHistory;
    };

    // ─── Run-level data ─────────────────────────────────────────────

    int totalSteps;
    bool allTargetsDestroyed;
    bool allSeekersDead;
    double maxNoiseLevel;

    std::vector<SeekerResult>      seekerResults;
    std::vector<TargetResult>      targetResults;
    std::vector<DetectorResult>    detectorResults;
    std::vector<InterceptorResult> interceptorResults;

    std::vector<PatrolDefenderResult> patrolDefenderResults; // per-run results for each patrol defender agent
    
    // ─── Summary statistics (filled by computeSummary) ──────────────

    int targetsDestroyed;
    int seekersThatReached;
    int seekersDetected;     // ever tracked
    int seekersIntercepted;  // killed by an interceptor
    double avgStepsToTarget;

    // ─── Methods ────────────────────────────────────────────────────

    void computeSummary();
    void print() const;
    void saveJSON(const std::string& filepath) const;
};

#endif // SIMRESULT_H