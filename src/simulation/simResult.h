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
 * Holds all output data from one simulation run.
 * Responsible for:
 *   - Storing per-seeker, per-target, and per-detector results
 *   - Computing summary statistics
 *   - Printing results to console
 *   - Saving results to JSON
 *
 * Does NOT run the simulation — that's Simulation's job.
 * Does NOT store map or spawn data — that's SpawnConfig's job.
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

        // Interception info
        bool intercepted;           // was destroyed by a detector?
        int interceptedByDetector;  // which detector (-1 if none)
        int interceptedAtStep;      // at which step (-1 if none)
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

    // ─── Per-detector output ────────────────────────────────────────

    struct DetectorResult {
        int id;
        int row;
        int col;
        double radius;
        int interceptCount;           // how many seekers destroyed

        struct Intercept {
            int seekerId;
            int step;
        };
        std::vector<Intercept> intercepts;  // detailed log
    };

    // ─── Run-level data ─────────────────────────────────────────────

    int totalSteps;
    bool allTargetsDestroyed;
    bool allSeekersDead;

    std::vector<SeekerResult> seekerResults;
    std::vector<TargetResult> targetResults;
    std::vector<DetectorResult> detectorResults;

    // ─── Summary statistics (computed from above) ───────────────────

    int targetsDestroyed;
    int seekersThatReached;
    int seekersIntercepted;
    double avgStepsToTarget;

    // ─── Methods ────────────────────────────────────────────────────

    /** Compute summary stats from the per-agent data. */
    void computeSummary();

    /** Print a human-readable summary to console. */
    void print() const;

    /** Save results to a JSON file. */
    void saveJSON(const std::string& filepath) const;
};

#endif // SIMRESULT_H