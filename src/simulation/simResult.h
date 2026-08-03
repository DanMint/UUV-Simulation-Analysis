#ifndef SIMRESULT_H
#define SIMRESULT_H

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

/**
 * SimResult
 *
 * Holds all output data from one simulation run. Each per-agent record keeps
 * both the broad category and the concrete type used to construct that agent.
 */
struct SimResult {

    // ─── Per-seeker output ──────────────────────────────────────────

    struct SeekerResult {
        int id;
        std::string category;
        std::string type;

        int stepsTaken;
        double pathCost;
        int nodesExpanded;
        bool reachedTarget;
        int targetId;
        std::vector<std::pair<int,int>> moveHistory;

        bool detected;
        int firstDetectedAtStep;
        int firstDetectedByDetector;

        bool intercepted;
        int interceptedByInterceptor;
        int interceptedAtStep;
    };

    // ─── Per-target output ──────────────────────────────────────────

    struct TargetResult {
        int id;
        std::string category;
        std::string type;

        int row;
        int col;
        bool destroyed;
        int destroyedAtStep;
        int destroyedBySeeker;
    };

    // ─── Per-detector output ────────────────────────────────────────

    struct DetectorResult {
        int id;
        std::string category;
        std::string type;

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

    // ─── Per-interceptor output ─────────────────────────────────────

    struct InterceptorResult {
        int id;
        std::string category;
        std::string type;

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

    // ─── Run-level data ─────────────────────────────────────────────

    int totalSteps;
    bool allTargetsDestroyed;
    bool allSeekersDead;
    double maxNoiseLevel;

    std::vector<SeekerResult> seekerResults;
    std::vector<TargetResult> targetResults;
    std::vector<DetectorResult> detectorResults;
    std::vector<InterceptorResult> interceptorResults;

    // ─── Summary statistics ─────────────────────────────────────────

    int targetsDestroyed;
    int seekersThatReached;
    int seekersDetected;
    int seekersIntercepted;
    double avgStepsToTarget;

    // ─── Methods ────────────────────────────────────────────────────

    void computeSummary();
    void print() const;
    void saveJSON(const std::string& filepath) const;
};

#endif // SIMRESULT_H