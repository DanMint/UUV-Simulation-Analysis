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

    // ── Cost-benefit summary (Lance's formula) ──────────────────────
    //   blue_cost = interceptor engagement spend = shots fired * cost per shot
    //               (every shot counts — hit or miss: "$2M interceptor vs $1000 drone")
    //   red_cost  = total unit cost of ALL attackers deployed (not just failures)
    //   loss_exchange_ratio = red / blue (<1 = defence efficient; >1 = attackers trade up)
    blueCost = 0.0f;
    for (const auto& ic : interceptorResults) {
        blueCost += ic.engagementCount * ic.engagementCost;
    }
    redCost = 0.0f;
    for (const auto& a : attackerResults) {
redCost += a.unitCostMin;
    }
    lossExchangeRatio = (blueCost > 0.0f) ? redCost / blueCost : 0.0f;

    // ── Vehicle-type cost breakdown ─────────────────────────────────
    // Group deployed attackers by vehicle type so the GA can see which
    // platforms drive red_cost (e.g. "$2M HUGIN vs $1000 QueenHornet").
    vehicleCostBreakdown.clear();
    for (const auto& a : attackerResults) {
        bool found = false;
        for (auto& v : vehicleCostBreakdown) {
            if (v.agentType == a.agentType) {
                v.count++;
                v.totalCost += a.unitCostMin;
                found = true;
                break;
            }
        }
        if (!found) {
            VehicleCostBreakdown v;
            v.agentType = a.agentType;
            v.count = 1;
            v.totalCost = a.unitCostMin;
            vehicleCostBreakdown.push_back(v);
        }
    }

    // ── GA fitness summary ──────────────────────────────────────────
    // P(detected) = (# attackers ever detected) / (total attackers)
    // P(killed)   = (# attackers intercepted/killed) / (total attackers)
    // effectiveness = P(detected) * P(killed)  (computed by the Python GA)
    // total_deployment_cost = Σ detector unitCost + Σ interceptor unitCost
    int totalAttackers = static_cast<int>(attackerResults.size());
    int attackersDetected = 0;
    int attackersKilled = 0;
    for (const auto& a : attackerResults) {
        if (!a.sightings.empty()) attackersDetected++;
        if (!a.intercepts.empty()) attackersKilled++;
    }
    probabilityDetected = (totalAttackers > 0) ? static_cast<double>(attackersDetected) / totalAttackers : 0.0;
    probabilityKilled   = (totalAttackers > 0) ? static_cast<double>(attackersKilled) / totalAttackers : 0.0;

    totalDetectorCost = 0.0f;
    for (const auto& d : detectorResults) {
        totalDetectorCost += d.unitCost;
    }
    totalInterceptorCost = 0.0f;
    for (const auto& ic : interceptorResults) {
        totalInterceptorCost += ic.unitCost;
    }
    totalDeploymentCost = totalDetectorCost + totalInterceptorCost;
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
    std::cout << "  Blue cost (engagement): $" << static_cast<long long>(blueCost) << std::endl;
    std::cout << "  Red cost (deployed):    $" << static_cast<long long>(redCost) << std::endl;
    std::cout << "  Loss exchange ratio:    " << lossExchangeRatio << std::endl;
    std::cout << "  P(detected):            " << probabilityDetected << std::endl;
    std::cout << "  P(killed):              " << probabilityKilled << std::endl;
    std::cout << "  Deployment cost:        $" << static_cast<long long>(totalDeploymentCost)
              << " (detectors=" << static_cast<long long>(totalDetectorCost)
              << ", interceptors=" << static_cast<long long>(totalInterceptorCost) << ")" << std::endl;

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
                  << " at (" << t.row << "," << t.col << ")"
                  << (t.isCritical ? " [CRITICAL]" : "") << ": ";
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
                      << ", cost " << d.unitCost
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
                      << ", vehicle " << (i.vehicleType.empty() ? "generic" : i.vehicleType)
                      << ", cost " << i.unitCost
                      << ": " << i.killCount << " kill(s), "
                      << i.engagementCount << " engagement(s)";
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
    jsonKey(buf, 2, "avg_steps_to_target",        avgStepsToTarget);                  buf << ",\n";
    jsonKey(buf, 2, "blue_cost",                  blueCost);                           buf << ",\n";
    jsonKey(buf, 2, "red_cost",                   redCost);                            buf << ",\n";
    jsonKey(buf, 2, "loss_exchange_ratio",        lossExchangeRatio);                  buf << ",\n";
    // ── GA fitness fields (parsed directly by the Python GA) ──
    jsonKey(buf, 2, "probability_detected",       probabilityDetected);               buf << ",\n";
    jsonKey(buf, 2, "probability_killed",         probabilityKilled);                 buf << ",\n";
    jsonKey(buf, 2, "total_detector_cost",        totalDetectorCost);                 buf << ",\n";
    jsonKey(buf, 2, "total_interceptor_cost",     totalInterceptorCost);             buf << ",\n";
    jsonKey(buf, 2, "total_deployment_cost",      totalDeploymentCost);              buf << ",\n";
    // -- Vehicle-type cost breakdown (per platform) --
    buf << "  \"vehicle_cost_breakdown\": [\n";
    for (size_t vi = 0; vi < vehicleCostBreakdown.size(); vi++) {
        const auto& v = vehicleCostBreakdown[vi];
        buf << "    {\n";
        jsonKeyStr(buf, 3, "agent_type", v.agentType);  buf << ",\n";
        jsonKey(buf, 3, "count",         v.count);      buf << ",\n";
        jsonKey(buf, 3, "total_cost",    v.totalCost);  buf << "\n";
        buf << "    }";
        if (vi < vehicleCostBreakdown.size() - 1) buf << ",";
        buf << "\n";
    }
    buf << "  ]\n";
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
        jsonKey(buf, 3, "unit_cost_min",           s.unitCostMin);                    buf << ",\n";
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
        jsonKey(buf, 3, "destroyed_by_seeker", t.destroyedBySeeker);   buf << ",\n";
        jsonKeyBool(buf, 3, "is_critical",  t.isCritical);             buf << "\n";
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
        jsonKey(buf, 3, "unit_cost",       d.unitCost);                       buf << ",\n";
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
        jsonKey(buf, 3, "engagement_count", ic.engagementCount);          buf << ",\n";
        jsonKey(buf, 3, "engagement_cost",  ic.engagementCost);           buf << ",\n";
        jsonKey(buf, 3, "unit_cost",        ic.unitCost);                  buf << ",\n";
        jsonKeyStr(buf, 3, "vehicle_type",  ic.vehicleType);              buf << ",\n";
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
        jsonKeyStr(buf, 3, "agent_type",   a.agentType);                      buf << ",\n";
        jsonKeyBool(buf, 3, "mission_success", a.missionSuccess);             buf << ",\n";
        jsonKey(buf, 3, "steps_taken",     a.stepsTaken);                     buf << ",\n";
        jsonKey(buf, 3, "path_cost",       a.pathCost);                       buf << ",\n";
        jsonKey(buf, 3, "nodes_expanded",  a.nodesExpanded);                  buf << ",\n";
        jsonKey(buf, 3, "target_id",       a.targetId);                       buf << ",\n";
        jsonKey(buf, 3, "kill_count",      a.killCount);                      buf << ",\n";
        jsonKey(buf, 3, "unit_cost_min",   a.unitCostMin);                    buf << ",\n";
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

// ════════════════════════════════════════════════════════════════════════════════
//  SAVE CSV  —  One row per run, appending to a shared summary file
// ════════════════════════════════════════════════════════════════════════════════
//
//  Cost-benefit row layout:
//    run_id, blue_cost, red_cost, loss_exchange_ratio, targets_destroyed,
//    total_targets, critical_asset_reached, total_steps, mission_success_rate,
//    interceptor_engagements
//
//  Lance's cost-benefit definitions (FIRST DRAFT — team to confirm):
//    - blue_cost = interceptor ENGAGEMENT spend = shots fired * cost per shot.
//                  Every shot counts — hit or miss ("$2M interceptor vs $1000
//                  drone" trade visible here).
//    - red_cost  = total unit cost of ALL attackers DEPLOYED (not just failed
//                  missions). Cost is sunk once a unit is committed.
//    - loss_exchange_ratio = red_cost / blue_cost. <1 = defence efficient;
//                  >1 = attackers trade up.
//    - critical_asset_reached = the DESIGNATED critical target (isCritical)
//                  was destroyed. Falls back to "any target destroyed" if
//                  no target is flagged critical.
//
// ════════════════════════════════════════════════════════════════════════════════

void SimResult::saveCSV(const std::string& filepath, int runId) const {
    // ── Compute per-run cost columns (Lance's formula) ──────────────
    float blueCost = 0.0f;
    for (const auto& ic : interceptorResults) {
        blueCost += ic.engagementCount * ic.engagementCost;
    }

    float redCost = 0.0f;
    for (const auto& a : attackerResults) {
        redCost += a.unitCostMin;  // cost of ALL attackers deployed
    }

    int totalTargets = static_cast<int>(targetResults.size());
    // Critical asset reach: use the DESIGNATED critical target (isCritical),
    // not the arbitrary first target. If no target is flagged critical,
    // fall back to "any target destroyed" for backwards compatibility.
    bool criticalAssetReached = false;
    bool hasCriticalFlag = false;
    for (const auto& t : targetResults) {
        if (t.isCritical) {
            hasCriticalFlag = true;
            if (t.destroyed) { criticalAssetReached = true; break; }
        }
    }
    if (!hasCriticalFlag) {
        for (const auto& t : targetResults) {
            if (t.destroyed) { criticalAssetReached = true; break; }
        }
    }

    float missionSuccessRate = 0.0f;
    if (!attackerResults.empty()) {
        int successes = 0;
        for (const auto& a : attackerResults) {
            if (a.missionSuccess) successes++;
        }
        missionSuccessRate = static_cast<float>(successes) / static_cast<float>(attackerResults.size());
    }

    // ── Append row (write header if file doesn't exist yet) ──────────
    std::ofstream file(filepath, std::ios::app);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open CSV file: " + filepath);
    }

    bool isNew = (std::ifstream(filepath).peek() == std::ifstream::traits_type::eof());
    if (isNew) {
        file << "run_id,blue_cost,red_cost,loss_exchange_ratio,targets_destroyed,total_targets,"
                "critical_asset_reached,total_steps,mission_success_rate,interceptor_engagements\n";
    }

    // Total interceptor shots fired across the whole defence
    int totalEngagements = 0;
    for (const auto& ic : interceptorResults) {
        totalEngagements += ic.engagementCount;
    }

    file << runId << ","
         << blueCost << ","
         << redCost << ","
         << lossExchangeRatio << ","
         << targetsDestroyed << ","
         << totalTargets << ","
         << (criticalAssetReached ? "true" : "false") << ","
         << totalSteps << ","
         << missionSuccessRate << ","
         << totalEngagements << "\n";
    file.close();

    std::cout << "CSV row appended to " << filepath
              << " (blue_cost=" << blueCost
              << ", red_cost=" << redCost
              << ", critical_asset_reached=" << (criticalAssetReached ? "true" : "false")
              << ")\n";
}

