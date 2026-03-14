#include "simResult.h"

// ════════════════════════════════════════════════════════════════════════════════
//  COMPUTE SUMMARY
// ════════════════════════════════════════════════════════════════════════════════

void SimResult::computeSummary() {
    targetsDestroyed = 0;
    seekersThatReached = 0;
    seekersIntercepted = 0;
    double totalStepsReached = 0;

    for (const auto& t : targetResults) {
        if (t.destroyed) targetsDestroyed++;
    }
    for (const auto& s : seekerResults) {
        if (s.reachedTarget) {
            seekersThatReached++;
            totalStepsReached += s.stepsTaken;
        }
        if (s.intercepted) {
            seekersIntercepted++;
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
    std::cout << "  Seekers intercepted: " << seekersIntercepted
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
        else if (s.intercepted)
            std::cout << " -> INTERCEPTED by detector " << s.interceptedByDetector
                      << " at step " << s.interceptedAtStep;
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

    if (!detectorResults.empty()) {
        std::cout << "\n  Detectors:" << std::endl;
        for (const auto& d : detectorResults) {
            std::cout << "    Detector " << d.id
                      << " at (" << d.row << "," << d.col
                      << "), radius " << d.radius
                      << ": " << d.interceptCount << " intercept(s)";
            if (d.interceptCount > 0) {
                std::cout << " [";
                for (int i = 0; i < static_cast<int>(d.intercepts.size()); i++) {
                    if (i > 0) std::cout << ", ";
                    std::cout << "seeker " << d.intercepts[i].seekerId
                              << " @ step " << d.intercepts[i].step;
                }
                std::cout << "]";
            }
            std::cout << std::endl;
        }
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
    file << "    \"seekers_intercepted\": " << seekersIntercepted << ",\n";
    file << "    \"total_seekers\": " << seekerResults.size() << ",\n";
    file << "    \"total_detectors\": " << detectorResults.size() << ",\n";
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
        file << "      \"intercepted\": "
             << (s.intercepted ? "true" : "false") << ",\n";
        file << "      \"intercepted_by_detector\": " << s.interceptedByDetector << ",\n";
        file << "      \"intercepted_at_step\": " << s.interceptedAtStep << ",\n";

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
    file << "  ],\n";

    // ── Per-detector results ─────────────────────────────────────────
    file << "  \"detectors\": [\n";
    for (int i = 0; i < (int)detectorResults.size(); i++) {
        const auto& d = detectorResults[i];
        file << "    {\n";
        file << "      \"id\": " << d.id << ",\n";
        file << "      \"row\": " << d.row << ",\n";
        file << "      \"col\": " << d.col << ",\n";
        file << "      \"radius\": " << d.radius << ",\n";
        file << "      \"intercept_count\": " << d.interceptCount << ",\n";

        file << "      \"intercepts\": [\n";
        for (int j = 0; j < (int)d.intercepts.size(); j++) {
            file << "        { \"seeker_id\": " << d.intercepts[j].seekerId
                 << ", \"step\": " << d.intercepts[j].step << " }";
            if (j < (int)d.intercepts.size() - 1) file << ",";
            file << "\n";
        }
        file << "      ]\n";

        file << "    }";
        if (i < (int)detectorResults.size() - 1) file << ",";
        file << "\n";
    }
    file << "  ]\n";

    file << "}\n";
    file.close();

    std::cout << "Results saved to " << filepath << "\n";
}