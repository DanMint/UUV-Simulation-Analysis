#include "simResult.h"
#include <sstream>
#include <string_view>

// ════════════════════════════════════════════════════════════════════════════════
//  JSON SERIALIZATION HELPERS  (file-scope)
// ════════════════════════════════════════════════════════════════════════════════
//
//  All output goes to an ostringstream buffer first, then flushed to disk
//  in a single I/O operation for significant performance improvement.
//
// ════════════════════════════════════════════════════════════════════════════════

namespace {

[[nodiscard]] static constexpr std::string_view jsonBool(bool v) noexcept {
    return v ? "true" : "false";
}

template <typename T>
static void jsonKey(std::ostream& os, int indent, std::string_view key, T value) {
    for (int i = 0; i < indent; i++) os << "  ";
    os << "\"" << key << "\": " << value;
}

static void jsonKeyStr(std::ostream& os, int indent, std::string_view key, std::string_view value) {
    for (int i = 0; i < indent; i++) os << "  ";
    os << "\"" << key << "\": \"" << value << "\"";
}

static void jsonKeyBool(std::ostream& os, int indent, std::string_view key, bool value) {
    for (int i = 0; i < indent; i++) os << "  ";
    os << "\"" << key << "\": " << jsonBool(value);
}

template <typename T>
static void jsonSightingArray(std::ostream& os, int indent,
                              std::string_view label,
                              const std::vector<T>& items) {
    for (int i = 0; i < indent; i++) os << "  ";
    os << "\"" << label << "\": [\n";
    for (size_t j = 0; j < items.size(); j++) {
        for (int i = 0; i < indent + 1; i++) os << "  ";
        os << "{ \"seeker_id\": " << items[j].seekerId
           << ", \"step\": " << items[j].step << " }";
        if (j < items.size() - 1) os << ",";
        os << "\n";
    }
    for (int i = 0; i < indent; i++) os << "  ";
    os << "]";
}

static void jsonPosArray(std::ostream& os, int indent,
                         std::string_view label,
                         const std::vector<std::pair<int,int>>& items) {
    for (int i = 0; i < indent; i++) os << "  ";
    os << "\"" << label << "\": [\n";
    for (size_t j = 0; j < items.size(); j++) {
        for (int i = 0; i < indent + 1; i++) os << "  ";
        os << "[" << items[j].first << ", " << items[j].second << "]";
        if (j < items.size() - 1) os << ",";
        os << "\n";
    }
    for (int i = 0; i < indent; i++) os << "  ";
    os << "]";
}

} // anonymous namespace

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

    attackersAlive = 0;
    for (const auto& a : attackerResults) {
        if (a.alive) attackersAlive++;
    }
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
    std::cout << "  Attackers active:    " << attackersAlive
              << " / " << attackerResults.size() << std::endl;
    std::cout << "  Avg steps to target: " << avgStepsToTarget << std::endl;

    std::cout << "\n  Seekers:" << std::endl;
    for (const auto& s : seekerResults) {
        std::cout << "    Seeker " << s.id << ": "
                  << s.stepsTaken << " steps, "
                  << s.nodesExpanded << " nodes expanded, "
                  << "path cost " << s.pathCost;
        if (s.reachedTarget) {
            std::cout << " -> reached target " << s.targetId;
        } else if (s.intercepted) {
            std::cout << " -> INTERCEPTED by interceptor " << s.interceptedByInterceptor
                      << " at step " << s.interceptedAtStep;
        } else {
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
                  << " at (" << t.row << "," << t.col << "): ";
        if (t.destroyed) {
            std::cout << "DESTROYED at step " << t.destroyedAtStep
                      << " by seeker " << t.destroyedBySeeker;
        } else {
            std::cout << "survived";
        }
        std::cout << std::endl;
    }

    if (!detectorResults.empty()) {
        std::cout << "\n  Detectors (sensors):" << std::endl;
        for (const auto& d : detectorResults) {
            std::cout << "    Detector " << d.id
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
                      << " at (" << i.row << "," << i.col
                      << "), kill radius " << i.killRadius
                      << ": " << i.killCount << " kill(s)";
            if (i.killCount > 0) {
                std::cout << " [";
                for (size_t k = 0; k < i.intercepts.size(); k++) {
                    if (k > 0) std::cout << ", ";
                    std::cout << "seeker " << i.intercepts[k].seekerId
                              << " @ step " << i.intercepts[k].step;
                }
                std::cout << "]";
            }
            std::cout << std::endl;
        }
    }

    if (!attackerResults.empty()) {
        std::cout << "\n  Attackers:" << std::endl;
        for (const auto& a : attackerResults) {
            std::cout << "    Attacker " << a.id
                      << " at (" << a.row << "," << a.col << ")"
                      << " state=" << a.state
                      << " alive=" << (a.alive ? "yes" : "no")
                      << " target=" << a.targetId
                      << " steps=" << a.stepsTaken
                      << " kills=" << a.killCount;
            if (a.missionSuccess) std::cout << " -> mission success";
            std::cout << std::endl;
            if (!a.sightings.empty()) {
                std::cout << "      sightings:";
                for (const auto& s : a.sightings) {
                    std::cout << " seeker " << s.seekerId
                              << " @" << s.step;
                }
                std::cout << std::endl;
            }
            if (!a.intercepts.empty()) {
                std::cout << "      intercepts:";
                for (const auto& it : a.intercepts) {
                    std::cout << " seeker " << it.seekerId
                              << " @" << it.step;
                }
                std::cout << std::endl;
            }
        }
    }

    std::cout << "=========================\n" << std::endl;
}

// ════════════════════════════════════════════════════════════════════════════════
//  SAVE JSON  —  Buffered for performance, templated helpers for DRY
// ════════════════════════════════════════════════════════════════════════════════

void SimResult::saveJSON(const std::string& filepath) const {
    std::ostringstream buf;

    buf << "{\n";

    // ── Summary ──
    buf << "  \"summary\": {\n";
    jsonKey(buf, 2, "total_steps",               totalSteps);                        buf << ",\n";
    jsonKey(buf, 2, "max_noise_level",            maxNoiseLevel);                     buf << ",\n";
    jsonKeyBool(buf, 2, "all_targets_destroyed",  allTargetsDestroyed);              buf << ",\n";
    jsonKey(buf, 2, "targets_destroyed",          targetsDestroyed);                  buf << ",\n";
    jsonKey(buf, 2, "total_targets",             static_cast<int>(targetResults.size())); buf << ",\n";
    jsonKey(buf, 2, "seekers_that_reached",       seekersThatReached);               buf << ",\n";
    jsonKey(buf, 2, "seekers_detected",           seekersDetected);                   buf << ",\n";
    jsonKey(buf, 2, "seekers_intercepted",        seekersIntercepted);               buf << ",\n";
    jsonKey(buf, 2, "total_seekers",             static_cast<int>(seekerResults.size()));   buf << ",\n";
    jsonKey(buf, 2, "total_detectors",           static_cast<int>(detectorResults.size())); buf << ",\n";
    jsonKey(buf, 2, "total_interceptors",        static_cast<int>(interceptorResults.size())); buf << ",\n";
    jsonKey(buf, 2, "total_attackers",           static_cast<int>(attackerResults.size()));  buf << ",\n";
    jsonKey(buf, 2, "attackers_alive",           attackersAlive);                     buf << ",\n";
    jsonKey(buf, 2, "avg_steps_to_target",        avgStepsToTarget);                  buf << "\n";
    buf << "  },\n";

    // ── Seekers ──
    buf << "  \"seekers\": [\n";
    for (size_t i = 0; i < seekerResults.size(); i++) {
        const auto& s = seekerResults[i];
        buf << "    {\n";
        jsonKey(buf, 3, "id",             s.id);                             buf << ",\n";
        jsonKey(buf, 3, "steps_taken",    s.stepsTaken);                      buf << ",\n";
        jsonKey(buf, 3, "path_cost",      s.pathCost);                       buf << ",\n";
        jsonKey(buf, 3, "nodes_expanded", s.nodesExpanded);                   buf << ",\n";
        jsonKeyBool(buf, 3, "reached_target", s.reachedTarget);               buf << ",\n";
        jsonKey(buf, 3, "target_id",      s.targetId);                        buf << ",\n";
        jsonKeyBool(buf, 3, "detected",            s.detected);               buf << ",\n";
        jsonKey(buf, 3, "first_detected_by_detector", s.firstDetectedByDetector); buf << ",\n";
        jsonKey(buf, 3, "first_detected_at_step",     s.firstDetectedAtStep);     buf << ",\n";
        jsonKeyBool(buf, 3, "intercepted",                s.intercepted);           buf << ",\n";
        jsonKey(buf, 3, "intercepted_by_interceptor",     s.interceptedByInterceptor); buf << ",\n";
        jsonKey(buf, 3, "intercepted_at_step",            s.interceptedAtStep);        buf << ",\n";
        jsonPosArray(buf, 3, "move_history", s.moveHistory); buf << "\n";
        buf << "    }";
        if (i < seekerResults.size() - 1) buf << ",";
        buf << "\n";
    }
    buf << "  ],\n";

    // ── Targets ──
    buf << "  \"targets\": [\n";
    for (size_t i = 0; i < targetResults.size(); i++) {
        const auto& t = targetResults[i];
        buf << "    {\n";
        jsonKey(buf, 3, "id",              t.id);                      buf << ",\n";
        jsonKey(buf, 3, "row",             t.row);                     buf << ",\n";
        jsonKey(buf, 3, "col",             t.col);                     buf << ",\n";
        jsonKeyBool(buf, 3, "destroyed",   t.destroyed);               buf << ",\n";
        jsonKey(buf, 3, "destroyed_at_step",   t.destroyedAtStep);     buf << ",\n";
        jsonKey(buf, 3, "destroyed_by_seeker", t.destroyedBySeeker);   buf << "\n";
        buf << "    }";
        if (i < targetResults.size() - 1) buf << ",";
        buf << "\n";
    }
    buf << "  ],\n";

    // ── Detectors ──
    buf << "  \"detectors\": [\n";
    for (size_t i = 0; i < detectorResults.size(); i++) {
        const auto& d = detectorResults[i];
        buf << "    {\n";
        jsonKey(buf, 3, "id",              d.id);                             buf << ",\n";
        jsonKey(buf, 3, "row",             d.row);                            buf << ",\n";
        jsonKey(buf, 3, "col",             d.col);                            buf << ",\n";
        jsonKey(buf, 3, "sensing_radius",  d.sensingRadius);                  buf << ",\n";
        jsonKey(buf, 3, "sighting_count",  d.sightingCount);                  buf << ",\n";
        jsonSightingArray(buf, 3, "sightings", d.sightings);                  buf << "\n";
        buf << "    }";
        if (i < detectorResults.size() - 1) buf << ",";
        buf << "\n";
    }
    buf << "  ],\n";

    // ── Interceptors ──
    buf << "  \"interceptors\": [\n";
    for (size_t i = 0; i < interceptorResults.size(); i++) {
        const auto& ic = interceptorResults[i];
        buf << "    {\n";
        jsonKey(buf, 3, "id",          ic.id);                             buf << ",\n";
        jsonKey(buf, 3, "row",         ic.row);                            buf << ",\n";
        jsonKey(buf, 3, "col",         ic.col);                            buf << ",\n";
        jsonKey(buf, 3, "kill_radius", ic.killRadius);                     buf << ",\n";
        jsonKey(buf, 3, "kill_count",  ic.killCount);                      buf << ",\n";
        jsonSightingArray(buf, 3, "intercepts", ic.intercepts);            buf << "\n";
        buf << "    }";
        if (i < interceptorResults.size() - 1) buf << ",";
        buf << "\n";
    }
    buf << "  ],\n";

    // ── Attackers ──
    buf << "  \"attackers\": [\n";
    for (size_t i = 0; i < attackerResults.size(); i++) {
        const auto& a = attackerResults[i];
        buf << "    {\n";
        jsonKey(buf, 3, "id",              a.id);                             buf << ",\n";
        jsonKey(buf, 3, "row",             a.row);                            buf << ",\n";
        jsonKey(buf, 3, "col",             a.col);                            buf << ",\n";
        jsonKeyBool(buf, 3, "alive",       a.alive);                          buf << ",\n";
        jsonKeyStr(buf, 3, "state",        a.state);                          buf << ",\n";
        jsonKeyBool(buf, 3, "mission_success", a.missionSuccess);             buf << ",\n";
        jsonKey(buf, 3, "steps_taken",     a.stepsTaken);                     buf << ",\n";
        jsonKey(buf, 3, "path_cost",       a.pathCost);                       buf << ",\n";
        jsonKey(buf, 3, "nodes_expanded",  a.nodesExpanded);                  buf << ",\n";
        jsonKey(buf, 3, "target_id",       a.targetId);                       buf << ",\n";
        jsonKey(buf, 3, "kill_count",      a.killCount);                      buf << ",\n";
        jsonSightingArray(buf, 3, "sightings",  a.sightings);                 buf << ",\n";
        jsonSightingArray(buf, 3, "intercepts", a.intercepts);                buf << ",\n";
        jsonPosArray(buf, 3, "move_history", a.moveHistory);                  buf << "\n";
        buf << "    }";
        if (i < attackerResults.size() - 1) buf << ",";
        buf << "\n";
    }
    buf << "  ]\n";

    buf << "}\n";

    // ── Single I/O flush to disk ──
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open results file: " + filepath);
    }
    file << buf.str();
    file.close();

    std::cout << "Results saved to " << filepath << " ("
              << buf.str().size() << " bytes)\n";
}

