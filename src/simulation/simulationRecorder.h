/**
 * @file simulationRecorder.h
 * @brief Full simulation state recorder for replay and web dashboard.
 *
 * Records the complete state of the simulation at each step, enabling:
 *   - Step-by-step replay in a web browser
 *   - Agent trajectory visualization
 *   - Detection/interception event timelines
 *   - Cost and effectiveness breakdowns over time
 *   - Real-time GA progress streaming
 */

#ifndef SIMULATION_RECORDER_H
#define SIMULATION_RECORDER_H

#include <vector>
#include <string>
#include <string_view>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <ostream>
#include "simulation.h"

/**
 * SimulationRecorder
 *
 * Captures a snapshot of every agent and the environment at each simulation
 * step. The recorded data is written to a JSON file that can be consumed by
 * the web dashboard for visualization and analysis.
 *
 * Data flow:
 *   1. Simulation calls recordStep() after each executeOneStepInternal()
 *   2. Recorder captures positions, states, and events
 *   3. On completion, saveJSON() writes the full recording to disk
 *
 * Thread safety: Not thread-safe. Use one recorder per simulation.
 */
class SimulationRecorder {
public:
    struct StepSnapshot {
        int step;
        std::vector<float> seekerX;
        std::vector<float> seekerY;
        std::vector<bool> seekerAlive;
        std::vector<bool> seekerDetected;
        std::vector<float> targetX;
        std::vector<float> targetY;
        std::vector<bool> targetAlive;
        std::vector<float> detectorX;
        std::vector<float> detectorY;
        std::vector<int> detectorSightings;
        std::vector<float> interceptorX;
        std::vector<float> interceptorY;
        std::vector<int> interceptorEngagements;
        std::vector<float> attackerX;
        std::vector<float> attackerY;
        std::vector<bool> attackerAlive;
        std::vector<int> attackerState;
        std::vector<int> events;  // packed event IDs for this step
    };

    struct RunMetadata {
        int runId;
        std::string scenarioName;
        std::string mapHash;
        int maxSteps;
        double noiseLevel;
        unsigned seed;
        std::string startTime;
        double wallTimeMs;
        int totalSeekers;
        int totalTargets;
        int totalDetectors;
        int totalInterceptors;
        int totalAttackers;
    };

    explicit SimulationRecorder(const RunMetadata& metadata);

    /** Record one simulation step. Call after executeOneStepInternal(). */
    void recordStep(const Simulation& sim, int step);

    /** Record a custom event (detection, intercept, etc.). */
    void recordEvent(int step, int eventType, int agentA, int agentB);

    /** Set event filter: only these event types are recorded. */
    void setEventFilter(int mask) noexcept { m_eventFilterMask = mask; }

    /** Get filtered events as a list of dicts with step/type/agent_a/agent_b. */
    std::vector<std::tuple<int, int, int, int>> filteredEvents() const;

    /** Get per-agent statistics collected during the run. */
    std::vector<std::tuple<int, std::string, int, int, int>> agentStats() const;

    /** Save the complete recording to a JSON file. */
    bool saveJSON(const std::string& filepath) const;

    /** Save as compact binary for fast loading. */
    bool saveBinary(const std::string& filepath) const;

    /** Clear all recorded data. */
    void clear() noexcept;

    size_t stepCount() const noexcept { return m_steps.size(); }
    const RunMetadata& metadata() const noexcept { return m_metadata; }
    bool hasData() const noexcept { return !m_steps.empty(); }

    static std::string timestampNow();

    static constexpr int EVENT_DETECTION = 1;
    static constexpr int EVENT_INTERCEPT = 2;
    static constexpr int EVENT_TARGET_DESTROYED = 4;
    static constexpr int EVENT_SEEKER_REACHED = 8;

private:
    RunMetadata m_metadata;
    std::vector<StepSnapshot> m_steps;
    std::vector<int> m_eventStream;
    int m_eventFilterMask{0xF};

    void serializeStep(std::ostringstream& oss, const StepSnapshot& step) const;
};

#endif // SIMULATION_RECORDER_H
