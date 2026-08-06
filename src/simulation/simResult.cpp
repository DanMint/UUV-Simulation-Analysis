#include "simResult.h"

// ════════════════════════════════════════════════════════════════════════════════
//  COMPUTE SUMMARY
// ════════════════════════════════════════════════════════════════════════════════

void SimResult::computeSummary() {
    targetsDestroyed = 0;
    seekersThatReached = 0;
    seekersDetected = 0;
    seekersIntercepted = 0;

    totalSeekerCost = 0;
    totalDefenderCost = 0;

    double totalStepsReached = 0.0;

    for (const auto& target : targetResults) {
        if (target.destroyed) {
            ++targetsDestroyed;
        }
    }

    for (const auto& seeker : seekerResults) {
        totalSeekerCost += seeker.cost;

        if (seeker.reachedTarget) {
            ++seekersThatReached;
            totalStepsReached += seeker.stepsTaken;
        }

        if (seeker.detected) {
            ++seekersDetected;
        }

        if (seeker.intercepted) {
            ++seekersIntercepted;
        }
    }

    for (const auto& detector : detectorResults) {
        totalDefenderCost += detector.cost;
    }

    for (const auto& interceptor : interceptorResults) {
        totalDefenderCost += interceptor.cost;
    }

    avgStepsToTarget = seekersThatReached > 0
        ? totalStepsReached / seekersThatReached
        : 0.0;
}

// ════════════════════════════════════════════════════════════════════════════════
//  PRINT
// ════════════════════════════════════════════════════════════════════════════════

void SimResult::print() const {
    std::cout << "\n=== Simulation Result ===" << std::endl;
    std::cout << "  Total steps:         " << totalSteps << std::endl;
    std::cout << "  Noise level:         " << maxNoiseLevel << std::endl;
    std::cout << "  Targets destroyed:   " << targetsDestroyed
              << " / " << targetResults.size() << std::endl;
    std::cout << "  Seekers reached:     " << seekersThatReached
              << " / " << seekerResults.size() << std::endl;
    std::cout << "  Seekers detected:    " << seekersDetected
              << " / " << seekerResults.size() << std::endl;
    std::cout << "  Seekers intercepted: " << seekersIntercepted
              << " / " << seekerResults.size() << std::endl;
    std::cout << "  Avg steps to target: " << avgStepsToTarget << std::endl;
    std::cout << "  Total seeker cost:   " << totalSeekerCost << std::endl;
    std::cout << "  Total defender cost: " << totalDefenderCost
              << " (detectors + interceptors)" << std::endl;

    std::cout << "\n  Seekers:" << std::endl;
    for (const auto& seeker : seekerResults) {
        std::cout << "    Seeker " << seeker.id
                  << " [" << seeker.category << "/" << seeker.type << "]"
                  << " cost=" << seeker.cost << ": "
                  << seeker.stepsTaken << " steps, "
                  << seeker.nodesExpanded << " nodes expanded, "
                  << "path cost " << seeker.pathCost;

        if (seeker.reachedTarget) {
            std::cout << " -> reached target " << seeker.targetId;
        }
        else if (seeker.intercepted) {
            std::cout << " -> INTERCEPTED by interceptor "
                      << seeker.interceptedByInterceptor
                      << " at step " << seeker.interceptedAtStep;
        }
        else {
            std::cout << " -> did NOT reach target";
        }

        if (seeker.detected) {
            std::cout << " (first tracked by detector "
                      << seeker.firstDetectedByDetector
                      << " @ step " << seeker.firstDetectedAtStep << ")";
        }

        std::cout << std::endl;
    }

    std::cout << "\n  Targets:" << std::endl;
    for (const auto& target : targetResults) {
        std::cout << "    Target " << target.id
                  << " [" << target.category << "/" << target.type << "]"
                  << " at (" << target.row << "," << target.col << "): ";

        if (target.destroyed) {
            std::cout << "DESTROYED at step " << target.destroyedAtStep
                      << " by seeker " << target.destroyedBySeeker;
        }
        else {
            std::cout << "survived";
        }

        std::cout << std::endl;
    }

    if (!detectorResults.empty()) {
        std::cout << "\n  Detectors (sensors):" << std::endl;

        for (const auto& detector : detectorResults) {
            std::cout << "    Detector " << detector.id
                      << " [" << detector.category << "/" << detector.type << "]"
                      << " cost=" << detector.cost
                      << " at (" << detector.row << "," << detector.col << ")"
                      << ", sensing radius " << detector.sensingRadius
                      << ": " << detector.sightingCount << " sighting(s)"
                      << std::endl;
        }
    }

    if (!interceptorResults.empty()) {
        std::cout << "\n  Interceptors (effectors):" << std::endl;

        for (const auto& interceptor : interceptorResults) {
            std::cout << "    Interceptor " << interceptor.id
                      << " [" << interceptor.category << "/"
                      << interceptor.type << "]"
                      << " cost=" << interceptor.cost
                      << " at (" << interceptor.row << "," << interceptor.col << ")"
                      << ", kill radius " << interceptor.killRadius
                      << ": " << interceptor.killCount << " kill(s)";

            if (interceptor.killCount > 0) {
                std::cout << " [";

                for (int index = 0;
                     index < static_cast<int>(interceptor.intercepts.size());
                     ++index) {
                    if (index > 0) {
                        std::cout << ", ";
                    }

                    std::cout << "seeker "
                              << interceptor.intercepts[index].seekerId
                              << " @ step "
                              << interceptor.intercepts[index].step;
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
        throw std::runtime_error(
            "Cannot open results file: " + filepath
        );
    }

    file << "{\n";

    // ── Summary ─────────────────────────────────────────────────────

    file << "  \"summary\": {\n";
    file << "    \"total_steps\": " << totalSteps << ",\n";
    file << "    \"max_noise_level\": " << maxNoiseLevel << ",\n";
    file << "    \"all_targets_destroyed\": "
         << (allTargetsDestroyed ? "true" : "false") << ",\n";
    file << "    \"targets_destroyed\": " << targetsDestroyed << ",\n";
    file << "    \"total_targets\": " << targetResults.size() << ",\n";
    file << "    \"seekers_that_reached\": " << seekersThatReached << ",\n";
    file << "    \"seekers_detected\": " << seekersDetected << ",\n";
    file << "    \"seekers_intercepted\": " << seekersIntercepted << ",\n";
    file << "    \"total_seekers\": " << seekerResults.size() << ",\n";
    file << "    \"total_detectors\": " << detectorResults.size() << ",\n";
    file << "    \"total_interceptors\": "
         << interceptorResults.size() << ",\n";
    file << "    \"total_seeker_cost\": " << totalSeekerCost << ",\n";
    file << "    \"total_defender_cost\": " << totalDefenderCost << ",\n";
    file << "    \"avg_steps_to_target\": " << avgStepsToTarget << "\n";
    file << "  },\n";

    // ── Seekers ─────────────────────────────────────────────────────

    file << "  \"seekers\": [\n";

    for (int index = 0;
         index < static_cast<int>(seekerResults.size());
         ++index) {
        const auto& seeker = seekerResults[index];

        file << "    {\n";
        file << "      \"id\": " << seeker.id << ",\n";
        file << "      \"category\": \"" << seeker.category << "\",\n";
        file << "      \"type\": \"" << seeker.type << "\",\n";
        file << "      \"cost\": " << seeker.cost << ",\n";
        file << "      \"steps_taken\": " << seeker.stepsTaken << ",\n";
        file << "      \"path_cost\": " << seeker.pathCost << ",\n";
        file << "      \"nodes_expanded\": " << seeker.nodesExpanded << ",\n";
        file << "      \"reached_target\": "
             << (seeker.reachedTarget ? "true" : "false") << ",\n";
        file << "      \"target_id\": " << seeker.targetId << ",\n";
        file << "      \"detected\": "
             << (seeker.detected ? "true" : "false") << ",\n";
        file << "      \"first_detected_by_detector\": "
             << seeker.firstDetectedByDetector << ",\n";
        file << "      \"first_detected_at_step\": "
             << seeker.firstDetectedAtStep << ",\n";
        file << "      \"intercepted\": "
             << (seeker.intercepted ? "true" : "false") << ",\n";
        file << "      \"intercepted_by_interceptor\": "
             << seeker.interceptedByInterceptor << ",\n";
        file << "      \"intercepted_at_step\": "
             << seeker.interceptedAtStep << ",\n";

        file << "      \"move_history\": [\n";

        for (int moveIndex = 0;
             moveIndex < static_cast<int>(seeker.moveHistory.size());
             ++moveIndex) {
            file << "        ["
                 << seeker.moveHistory[moveIndex].first
                 << ", "
                 << seeker.moveHistory[moveIndex].second
                 << "]";

            if (moveIndex <
                static_cast<int>(seeker.moveHistory.size()) - 1) {
                file << ",";
            }

            file << "\n";
        }

        file << "      ]\n";
        file << "    }";

        if (index < static_cast<int>(seekerResults.size()) - 1) {
            file << ",";
        }

        file << "\n";
    }

    file << "  ],\n";

    // ── Targets ─────────────────────────────────────────────────────

    file << "  \"targets\": [\n";

    for (int index = 0;
         index < static_cast<int>(targetResults.size());
         ++index) {
        const auto& target = targetResults[index];

        file << "    {\n";
        file << "      \"id\": " << target.id << ",\n";
        file << "      \"category\": \"" << target.category << "\",\n";
        file << "      \"type\": \"" << target.type << "\",\n";
        file << "      \"row\": " << target.row << ",\n";
        file << "      \"col\": " << target.col << ",\n";
        file << "      \"destroyed\": "
             << (target.destroyed ? "true" : "false") << ",\n";
        file << "      \"destroyed_at_step\": "
             << target.destroyedAtStep << ",\n";
        file << "      \"destroyed_by_seeker\": "
             << target.destroyedBySeeker << "\n";
        file << "    }";

        if (index < static_cast<int>(targetResults.size()) - 1) {
            file << ",";
        }

        file << "\n";
    }

    file << "  ],\n";

    // ── Detectors ───────────────────────────────────────────────────

    file << "  \"detectors\": [\n";

    for (int index = 0;
         index < static_cast<int>(detectorResults.size());
         ++index) {
        const auto& detector = detectorResults[index];

        file << "    {\n";
        file << "      \"id\": " << detector.id << ",\n";
        file << "      \"category\": \"" << detector.category << "\",\n";
        file << "      \"type\": \"" << detector.type << "\",\n";
        file << "      \"cost\": " << detector.cost << ",\n";
        file << "      \"row\": " << detector.row << ",\n";
        file << "      \"col\": " << detector.col << ",\n";
        file << "      \"sensing_radius\": "
             << detector.sensingRadius << ",\n";
        file << "      \"sighting_count\": "
             << detector.sightingCount << ",\n";

        file << "      \"sightings\": [\n";

        for (int sightingIndex = 0;
             sightingIndex < static_cast<int>(detector.sightings.size());
             ++sightingIndex) {
            file << "        { \"seeker_id\": "
                 << detector.sightings[sightingIndex].seekerId
                 << ", \"step\": "
                 << detector.sightings[sightingIndex].step
                 << " }";

            if (sightingIndex <
                static_cast<int>(detector.sightings.size()) - 1) {
                file << ",";
            }

            file << "\n";
        }

        file << "      ]\n";
        file << "    }";

        if (index < static_cast<int>(detectorResults.size()) - 1) {
            file << ",";
        }

        file << "\n";
    }

    file << "  ],\n";

    // ── Interceptors ────────────────────────────────────────────────

    file << "  \"interceptors\": [\n";

    for (int index = 0;
         index < static_cast<int>(interceptorResults.size());
         ++index) {
        const auto& interceptor = interceptorResults[index];

        file << "    {\n";
        file << "      \"id\": " << interceptor.id << ",\n";
        file << "      \"category\": \"" << interceptor.category << "\",\n";
        file << "      \"type\": \"" << interceptor.type << "\",\n";
        file << "      \"cost\": " << interceptor.cost << ",\n";
        file << "      \"row\": " << interceptor.row << ",\n";
        file << "      \"col\": " << interceptor.col << ",\n";
        file << "      \"kill_radius\": "
             << interceptor.killRadius << ",\n";
        file << "      \"kill_count\": "
             << interceptor.killCount << ",\n";

        file << "      \"intercepts\": [\n";

        for (int interceptIndex = 0;
             interceptIndex < static_cast<int>(interceptor.intercepts.size());
             ++interceptIndex) {
            file << "        { \"seeker_id\": "
                 << interceptor.intercepts[interceptIndex].seekerId
                 << ", \"step\": "
                 << interceptor.intercepts[interceptIndex].step
                 << " }";

            if (interceptIndex <
                static_cast<int>(interceptor.intercepts.size()) - 1) {
                file << ",";
            }

            file << "\n";
        }

        file << "      ]\n";
        file << "    }";

        if (index < static_cast<int>(interceptorResults.size()) - 1) {
            file << ",";
        }

        file << "\n";
    }

    file << "  ]\n";
    file << "}\n";

    std::cout << "Results saved to " << filepath << "\n";
}