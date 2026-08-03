#include "simResult.h"

// ════════════════════════════════════════════════════════════════════════════════
//  COMPUTE SUMMARY
// ════════════════════════════════════════════════════════════════════════════════

void SimResult::computeSummary() {
    targetsDestroyed = 0;
    seekersThatReached = 0;
    seekersDetected = 0;
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
        if (s.detected)    seekersDetected++;
        if (s.intercepted) seekersIntercepted++;
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

    std::cout << "\n  Seekers:" << std::endl;
    for (const auto& s : seekerResults) {
        std::cout << "    Seeker " << s.id
                  << " [" << s.category << "/" << s.type << "]: "
                  << s.stepsTaken << " steps, "
                  << s.nodesExpanded << " nodes expanded, "
                  << "path cost " << s.pathCost;
        if (s.reachedTarget) {
            std::cout << " -> reached target " << s.targetId;
        } 
        
        else if (s.intercepted) {
            std::cout << " -> INTERCEPTED by interceptor " << s.interceptedByInterceptor
                      << " at step " << s.interceptedAtStep;
        } 
        else {
            std::cout << " -> did NOT reach target";
        }

        if (s.detected) {
            std::cout << " (first tracked by detector " << s.firstDetectedByDetector
                      << " @ step " << s.firstDetectedAtStep << ")";
        }
        std::cout << std::endl;
    }

    std::cout << "\n  Targets:" << std::endl;
    for (const auto& t : targetResults) {
        std::cout << "    Target " << t.id
                  << " [" << t.category << "/" << t.type << "]"
                  << " at (" << t.row << "," << t.col << "): ";
        if (t.destroyed) {
            std::cout << "DESTROYED at step " << t.destroyedAtStep
                      << " by seeker " << t.destroyedBySeeker;
        } 
        else {
            std::cout << "survived";
        }
        std::cout << std::endl;
    }

    if (!detectorResults.empty()) {
        std::cout << "\n  Detectors (sensors):" << std::endl;
        for (const auto& d : detectorResults) {
            std::cout << "    Detector " << d.id
                      << " [" << d.category << "/" << d.type << "]"
                      << " at (" << d.row << "," << d.col
                      << "), sensing radius " << d.sensingRadius
                      << ": " << d.sightingCount << " sighting(s)";
            std::cout << std::endl;
        }
    }

    if (!interceptorResults.empty()) {
        std::cout << "\n  Interceptors (effectors):" << std::endl;
        for (const auto& i : interceptorResults) {
            std::cout << "    Interceptor " << i.id
                      << " [" << i.category << "/" << i.type << "]"
                      << " at (" << i.row << "," << i.col
                      << "), kill radius " << i.killRadius
                      << ": " << i.killCount << " kill(s)";
            if (i.killCount > 0) {
                std::cout << " [";
                for (int k = 0; k < static_cast<int>(i.intercepts.size()); k++) {
                    if (k > 0) 
                        std::cout << ", ";
                        
                    std::cout << "seeker " << i.intercepts[k].seekerId
                              << " @ step " << i.intercepts[k].step;
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

    // ── Summary ──
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
    file << "    \"total_interceptors\": " << interceptorResults.size() << ",\n";
    file << "    \"avg_steps_to_target\": " << avgStepsToTarget << "\n";
    file << "  },\n";

    // ── Seekers ──
    file << "  \"seekers\": [\n";
    for (int i = 0; i < (int)seekerResults.size(); i++) {
        const auto& s = seekerResults[i];
        file << "    {\n";
        file << "      \"id\": " << s.id << ",\n";
        file << "      \"category\": \"" << s.category << "\",\n";
        file << "      \"type\": \"" << s.type << "\",\n";
        file << "      \"steps_taken\": " << s.stepsTaken << ",\n";
        file << "      \"path_cost\": " << s.pathCost << ",\n";
        file << "      \"nodes_expanded\": " << s.nodesExpanded << ",\n";
        file << "      \"reached_target\": "
             << (s.reachedTarget ? "true" : "false") << ",\n";
        file << "      \"target_id\": " << s.targetId << ",\n";

        file << "      \"detected\": "
             << (s.detected ? "true" : "false") << ",\n";
        file << "      \"first_detected_by_detector\": "
             << s.firstDetectedByDetector << ",\n";
        file << "      \"first_detected_at_step\": "
             << s.firstDetectedAtStep << ",\n";

        file << "      \"intercepted\": "
             << (s.intercepted ? "true" : "false") << ",\n";
        file << "      \"intercepted_by_interceptor\": "
             << s.interceptedByInterceptor << ",\n";
        file << "      \"intercepted_at_step\": "
             << s.interceptedAtStep << ",\n";

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

    // ── Targets ──
    file << "  \"targets\": [\n";
    for (int i = 0; i < (int)targetResults.size(); i++) {
        const auto& t = targetResults[i];
        file << "    {\n";
        file << "      \"id\": " << t.id << ",\n";
        file << "      \"category\": \"" << t.category << "\",\n";
        file << "      \"type\": \"" << t.type << "\",\n";
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

    // ── Detectors ──
    file << "  \"detectors\": [\n";
    for (int i = 0; i < (int)detectorResults.size(); i++) {
        const auto& d = detectorResults[i];
        file << "    {\n";
        file << "      \"id\": " << d.id << ",\n";
        file << "      \"category\": \"" << d.category << "\",\n";
        file << "      \"type\": \"" << d.type << "\",\n";
        file << "      \"row\": " << d.row << ",\n";
        file << "      \"col\": " << d.col << ",\n";
        file << "      \"sensing_radius\": " << d.sensingRadius << ",\n";
        file << "      \"sighting_count\": " << d.sightingCount << ",\n";

        file << "      \"sightings\": [\n";
        for (int j = 0; j < (int)d.sightings.size(); j++) {
            file << "        { \"seeker_id\": " << d.sightings[j].seekerId
                 << ", \"step\": " << d.sightings[j].step << " }";
            if (j < (int)d.sightings.size() - 1) file << ",";
            file << "\n";
        }
        file << "      ]\n";

        file << "    }";
        if (i < (int)detectorResults.size() - 1) file << ",";
        file << "\n";
    }
    file << "  ],\n";

    // ── Interceptors ──
    file << "  \"interceptors\": [\n";
    for (int i = 0; i < (int)interceptorResults.size(); i++) {
        const auto& it = interceptorResults[i];
        file << "    {\n";
        file << "      \"id\": " << it.id << ",\n";
        file << "      \"category\": \"" << it.category << "\",\n";
        file << "      \"type\": \"" << it.type << "\",\n";
        file << "      \"row\": " << it.row << ",\n";
        file << "      \"col\": " << it.col << ",\n";
        file << "      \"kill_radius\": " << it.killRadius << ",\n";
        file << "      \"kill_count\": " << it.killCount << ",\n";

        file << "      \"intercepts\": [\n";
        for (int j = 0; j < (int)it.intercepts.size(); j++) {
            file << "        { \"seeker_id\": " << it.intercepts[j].seekerId
                 << ", \"step\": " << it.intercepts[j].step << " }";
            if (j < (int)it.intercepts.size() - 1) file << ",";
            file << "\n";
        }
        file << "      ]\n";

        file << "    }";
        if (i < (int)interceptorResults.size() - 1) file << ",";
        file << "\n";
    }
    file << "  ]\n";

    file << "}\n";
    file.close();

    std::cout << "Results saved to " << filepath << "\n";
}