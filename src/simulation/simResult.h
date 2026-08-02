#ifndef SIMRESULT_H
#define SIMRESULT_H

#include <vector>
#include <string>
#include <string_view>
#include <utility>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <concepts>

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
 *
 * Performance: saveJSON() uses an ostringstream buffer to build the entire
 * JSON document before writing to disk in a single I/O operation. This is
 * significantly faster than hundreds of individual file << calls.
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

    // Unit cost of this agent's vehicle (for cost-benefit CSV export).
    // Populated in Simulation::buildResult() from VehicleSpecs::unitCostMin.
    float unitCostMin = 0.0f;
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

    struct AttackerResult {
        int id;
        int row;
        int col;
        bool alive;
        std::string state;
        std::string agentType;   ///< vehicle type key (e.g. "hugin", "tb2")
        bool missionSuccess;
        int stepsTaken;
        double pathCost;
        int nodesExpanded;
        int targetId;
        int killCount;

        struct Sighting {
            int seekerId;
            int step;
        };
        std::vector<Sighting> sightings;

        struct Intercept {
            int seekerId;
            int step;
        };
        std::vector<Intercept> intercepts;

        std::vector<std::pair<int,int>> moveHistory;

        // Unit cost of this agent's vehicle (for cost-benefit CSV export).
        // Populated in Simulation::buildResult() from VehicleSpecs::unitCostMin.
        float unitCostMin = 0.0f;
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
    std::vector<AttackerResult>    attackerResults;

    // ─── Summary statistics (filled by computeSummary) ──────────────

    int targetsDestroyed;
    int seekersThatReached;
    int seekersDetected;     // ever tracked
    int seekersIntercepted;  // killed by an interceptor
    int attackersAlive;
    double avgStepsToTarget;

    // ─── Cost-benefit summary (filled by computeSummary) ────────────

    float blueCost;          ///< Sum of unitCostMin over seekers intercepted (defence loss)
    float redCost;           ///< Sum of unitCostMin over attackers that failed mission (offence waste)
    float lossExchangeRatio; ///< redCost / blueCost (>1 = attackers trade up; <1 = defence efficient)

    // ─── Methods ────────────────────────────────────────────────────

    void computeSummary();
    void print() const;
    void saveJSON(const std::string& filepath) const;

    /**
     * Append one cost-benefit row to a CSV file (creates the header row
     * on first call). Columns, in order:
     *   run_id, blue_cost, red_cost, targets_destroyed, total_targets,
     *   critical_asset_reached, total_steps, mission_success_rate
     *
     * Cost definitions (FIRST DRAFT — placeholder assumptions to be
     * confirmed by the team):
     *   - blue_cost  = sum of unitCostMin across all seekers that were
     *                  intercepted (lost). NOTE: in the current codebase
     *                  only seekers/attackers carry VehicleSpecs costs;
     *                  targets/detectors/interceptors have no cost data
     *                  yet, so this is a proxy until the team defines
     *                  defender-side costs.
     *   - red_cost   = sum of unitCostMin across all attackers that did
     *                  NOT achieve missionSuccess (i.e. their cost was
     *                  "wasted").
     *   - critical_asset_reached = targetResults[0].destroyed. NOTE:
     *                  simplification — the codebase has no dedicated
     *                  critical-asset flag yet.
     */
    void saveCSV(const std::string& filepath, int runId) const;
};

#endif // SIMRESULT_H