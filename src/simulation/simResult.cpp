#include "simResult.h"

// ════════════════════════════════════════════════════════════════════════════════
//  COMPUTE SUMMARY
// ════════════════════════════════════════════════════════════════════════════════

void SimResult::computeSummary() {
    targetsDestroyed = 0;
    seekersThatReached = 0;
    double totalStepsReached = 0;

    for (const auto& t : targetResults) {
        if (t.destroyed) targetsDestroyed++;
    }
    for (const auto& s : seekerResults) {
        if (s.reachedTarget) {
            seekersThatReached++;
            totalStepsReached += s.stepsTaken;
        }
    }
    avgStepsToTarget = (seekersThatReached > 0)
        ? totalStepsReached / seekersThatReached
        : 0.0;
}

// ════════════════════════════════════════════════════════════════════════════════
//  PRINT
// ════════════════════════════════════════════════════════════════════════════════

void SimResult::print() const {
    std::cout << "\n=== Simulation Result ===" << std::endl;
    std::cout << "  Total steps:         " << totalSteps << std::endl;
    std::cout << "  Targets destroyed:   " << targetsDestroyed
              << " / " << targetResults.size() << std::endl;
    std::cout << "  Seekers reached:     " << seekersThatReached
              << " / " << seekerResults.size() << std::endl;
    std::cout << "  Avg steps to target: " << avgStepsToTarget << std::endl;

    std::cout << "\n  Seekers:" << std::endl;
    for (const auto& s : seekerResults) {
        std::cout << "    Seeker " << s.id << ": "
                  << s.stepsTaken << " steps, "
                  << s.nodesExpanded << " nodes expanded, "
                  << "path cost " << s.pathCost;
        if (s.reachedTarget)
            std::cout << " -> reached target " << s.targetId;
        else
            std::cout << " -> did NOT reach target";
        std::cout << std::endl;
    }

    std::cout << "\n  Targets:" << std::endl;
    for (const auto& t : targetResults) {
        std::cout << "    Target " << t.id
                  << " at (" << t.row << "," << t.col << "): ";
        if (t.destroyed)
            std::cout << "DESTROYED at step " << t.destroyedAtStep
                      << " by seeker " << t.destroyedBySeeker;
        else
            std::cout << "survived";
        std::cout << std::endl;
    }
    std::cout << "=========================\n" << std::endl;
}

// ════════════════════════════════════════════════════════════════════════════════
//  SAVE JSON
// ════════════════════════════════════════════════════════════════════════════════

void SimResult::saveJSON(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open results file: " + filepath);
    }

    file << "{\n";

    // ── Summary ──────────────────────────────────────────────────────
    file << "  \"summary\": {\n";
    file << "    \"total_steps\": " << totalSteps << ",\n";
    file << "    \"all_targets_destroyed\": "
         << (allTargetsDestroyed ? "true" : "false") << ",\n";
    file << "    \"targets_destroyed\": " << targetsDestroyed << ",\n";
    file << "    \"total_targets\": " << targetResults.size() << ",\n";
    file << "    \"seekers_that_reached\": " << seekersThatReached << ",\n";
    file << "    \"total_seekers\": " << seekerResults.size() << ",\n";
    file << "    \"avg_steps_to_target\": " << avgStepsToTarget << "\n";
    file << "  },\n";

    // ── Per-seeker results ───────────────────────────────────────────
    file << "  \"seekers\": [\n";
    for (int i = 0; i < (int)seekerResults.size(); i++) {
        const auto& s = seekerResults[i];
        file << "    {\n";
        file << "      \"id\": " << s.id << ",\n";
        file << "      \"steps_taken\": " << s.stepsTaken << ",\n";
        file << "      \"path_cost\": " << s.pathCost << ",\n";
        file << "      \"nodes_expanded\": " << s.nodesExpanded << ",\n";
        file << "      \"reached_target\": "
             << (s.reachedTarget ? "true" : "false") << ",\n";
        file << "      \"target_id\": " << s.targetId << ",\n";

        // Move history: every position the seeker visited
        file << "      \"move_history\": [\n";
        for (int j = 0; j < (int)s.moveHistory.size(); j++) {
            file << "        [" << s.moveHistory[j].first
                 << ", " << s.moveHistory[j].second << "]";
            if (j < (int)s.moveHistory.size() - 1) file << ",";
            file << "\n";
        }
        file << "      ]\n";

        file << "    }";
        if (i < (int)seekerResults.size() - 1) file << ",";
        file << "\n";
    }
    file << "  ],\n";

    // ── Per-target results ───────────────────────────────────────────
    file << "  \"targets\": [\n";
    for (int i = 0; i < (int)targetResults.size(); i++) {
        const auto& t = targetResults[i];
        file << "    {\n";
        file << "      \"id\": " << t.id << ",\n";
        file << "      \"row\": " << t.row << ",\n";
        file << "      \"col\": " << t.col << ",\n";
        file << "      \"destroyed\": "
             << (t.destroyed ? "true" : "false") << ",\n";
        file << "      \"destroyed_at_step\": " << t.destroyedAtStep << ",\n";
        file << "      \"destroyed_by_seeker\": " << t.destroyedBySeeker << "\n";
        file << "    }";
        if (i < (int)targetResults.size() - 1) file << ",";
        file << "\n";
    }
    file << "  ]\n";

    file << "}\n";
    file.close();

    std::cout << "Results saved to " << filepath << "\n";
}