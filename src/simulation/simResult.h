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
 *   - Storing per-seeker and per-target results
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

    // ─── Run-level data ─────────────────────────────────────────────

    int totalSteps;
    bool allTargetsDestroyed;
    bool allSeekersDead;

    std::vector<SeekerResult> seekerResults;
    std::vector<TargetResult> targetResults;

    // ─── Summary statistics (computed from above) ───────────────────

    int targetsDestroyed;
    int seekersThatReached;
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