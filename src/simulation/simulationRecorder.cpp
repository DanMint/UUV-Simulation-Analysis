#include "simulationRecorder.h"
#include <iomanip>
#include <cstring>

// ════════════════════════════════════════════════════════════════════════════════
//  CONSTRUCTOR
// ════════════════════════════════════════════════════════════════════════════════

SimulationRecorder::SimulationRecorder(const RunMetadata& metadata)
    : m_metadata(metadata)
{
    m_steps.reserve(metadata.maxSteps);
    m_eventStream.reserve(1024);
}

// ════════════════════════════════════════════════════════════════════════════════
//  RECORDING
// ════════════════════════════════════════════════════════════════════════════════

void SimulationRecorder::recordStep(const Simulation& sim, int step) {
    StepSnapshot snap;
    snap.step = step;

    const auto& seekers = sim.getSeekers();
    const auto& targets = sim.getTargets();
    const auto& detectors = sim.getDetectors();
    const auto& interceptors = sim.getInterceptors();
    const auto& attackers = sim.getAttackers();

    snap.seekerX.reserve(seekers.size());
    snap.seekerY.reserve(seekers.size());
    snap.seekerAlive.reserve(seekers.size());
    snap.seekerDetected.reserve(seekers.size());
    for (const auto& s : seekers) {
        snap.seekerX.push_back(static_cast<float>(s.col));
        snap.seekerY.push_back(static_cast<float>(s.row));
        snap.seekerAlive.push_back(s.alive);
        snap.seekerDetected.push_back(s.detected);
    }

    snap.targetX.reserve(targets.size());
    snap.targetY.reserve(targets.size());
    snap.targetAlive.reserve(targets.size());
    for (const auto& t : targets) {
        snap.targetX.push_back(static_cast<float>(t.col));
        snap.targetY.push_back(static_cast<float>(t.row));
        snap.targetAlive.push_back(t.alive);
    }

    snap.detectorX.reserve(detectors.size());
    snap.detectorY.reserve(detectors.size());
    snap.detectorSightings.reserve(detectors.size());
    for (const auto& d : detectors) {
        snap.detectorX.push_back(static_cast<float>(d.col));
        snap.detectorY.push_back(static_cast<float>(d.row));
        snap.detectorSightings.push_back(d.sightingCount);
    }

    snap.interceptorX.reserve(interceptors.size());
    snap.interceptorY.reserve(interceptors.size());
    snap.interceptorEngagements.reserve(interceptors.size());
    for (const auto& i : interceptors) {
        snap.interceptorX.push_back(static_cast<float>(i.col));
        snap.interceptorY.push_back(static_cast<float>(i.row));
        snap.interceptorEngagements.push_back(i.engagementCount);
    }

    snap.attackerX.reserve(attackers.size());
    snap.attackerY.reserve(attackers.size());
    snap.attackerAlive.reserve(attackers.size());
    snap.attackerState.reserve(attackers.size());
    for (const auto& a : attackers) {
        snap.attackerX.push_back(static_cast<float>(a.col));
        snap.attackerY.push_back(static_cast<float>(a.row));
        snap.attackerAlive.push_back(a.alive);
        snap.attackerState.push_back(static_cast<int>(a.fsmState));
    }

    snap.events.reserve(m_eventStream.size());
    m_steps.push_back(std::move(snap));
}

void SimulationRecorder::recordEvent(int step, int eventType, int agentA, int agentB) {
    m_eventStream.push_back(step);
    m_eventStream.push_back(eventType);
    m_eventStream.push_back(agentA);
    m_eventStream.push_back(agentB);
}

// ════════════════════════════════════════════════════════════════════════════════
//  SERIALIZATION
// ════════════════════════════════════════════════════════════════════════════════

std::string SimulationRecorder::timestampNow() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto t = system_clock::to_time_t(now);
    std::ostringstream oss;
    std::tm tm{};
    gmtime_s(&tm, &t);
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

void SimulationRecorder::serializeStep(std::ostringstream& oss, const StepSnapshot& step) const {
    oss << "{\"step\":" << step.step;

    // Seekers
    oss << ",\"seekers\":{";
    for (size_t i = 0; i < step.seekerX.size(); ++i) {
        if (i) oss << ",";
        oss << "\"" << i << "\":{";
        oss << "\"x\":" << step.seekerX[i] << ",\"y\":" << step.seekerY[i];
        oss << ",\"alive\":" << (step.seekerAlive[i] ? "true" : "false");
        oss << ",\"detected\":" << (step.seekerDetected[i] ? "true" : "false");
        oss << "}";
    }
    oss << "}";

    // Targets
    oss << ",\"targets\":{";
    for (size_t i = 0; i < step.targetX.size(); ++i) {
        if (i) oss << ",";
        oss << "\"" << i << "\":{";
        oss << "\"x\":" << step.targetX[i] << ",\"y\":" << step.targetY[i];
        oss << ",\"alive\":" << (step.targetAlive[i] ? "true" : "false");
        oss << "}";
    }
    oss << "}";

    // Detectors
    oss << ",\"detectors\":{";
    for (size_t i = 0; i < step.detectorX.size(); ++i) {
        if (i) oss << ",";
        oss << "\"" << i << "\":{";
        oss << "\"x\":" << step.detectorX[i] << ",\"y\":" << step.detectorY[i];
        oss << ",\"sightings\":" << step.detectorSightings[i];
        oss << "}";
    }
    oss << "}";

    // Interceptors
    oss << ",\"interceptors\":{";
    for (size_t i = 0; i < step.interceptorX.size(); ++i) {
        if (i) oss << ",";
        oss << "\"" << i << "\":{";
        oss << "\"x\":" << step.interceptorX[i] << ",\"y\":" << step.interceptorY[i];
        oss << ",\"engagements\":" << step.interceptorEngagements[i];
        oss << "}";
    }
    oss << "}";

    // Attackers
    oss << ",\"attackers\":{";
    for (size_t i = 0; i < step.attackerX.size(); ++i) {
        if (i) oss << ",";
        oss << "\"" << i << "\":{";
        oss << "\"x\":" << step.attackerX[i] << ",\"y\":" << step.attackerY[i];
        oss << ",\"alive\":" << (step.attackerAlive[i] ? "true" : "false");
        oss << ",\"state\":" << step.attackerState[i];
        oss << "}";
    }
    oss << "}";

    // Events for this step
    oss << ",\"events\":[";
    bool firstEvent = true;
    for (size_t i = 0; i < m_eventStream.size(); i += 4) {
        if (m_eventStream[i] != step.step) continue;
        if (!firstEvent) oss << ",";
        oss << "{\"type\":" << m_eventStream[i + 1];
        oss << ",\"a\":" << m_eventStream[i + 2];
        oss << ",\"b\":" << m_eventStream[i + 3];
        oss << "}";
        firstEvent = false;
    }
    oss << "]";

    oss << "}";
}

bool SimulationRecorder::saveJSON(const std::string& filepath) const {
    std::ofstream ofs(filepath);
    if (!ofs.is_open()) return false;

    std::ostringstream oss;
    oss << "{\"metadata\":{";
    oss << "\"runId\":" << m_metadata.runId << ",";
    oss << "\"scenarioName\":\"" << m_metadata.scenarioName << "\",";
    oss << "\"mapHash\":\"" << m_metadata.mapHash << "\",";
    oss << "\"maxSteps\":" << m_metadata.maxSteps << ",";
    oss << "\"noiseLevel\":" << m_metadata.noiseLevel << ",";
    oss << "\"seed\":" << m_metadata.seed << ",";
    oss << "\"startTime\":\"" << m_metadata.startTime << "\",";
    oss << "\"wallTimeMs\":" << m_metadata.wallTimeMs << ",";
    oss << "\"totalSeekers\":" << m_metadata.totalSeekers << ",";
    oss << "\"totalTargets\":" << m_metadata.totalTargets << ",";
    oss << "\"totalDetectors\":" << m_metadata.totalDetectors << ",";
    oss << "\"totalInterceptors\":" << m_metadata.totalInterceptors << ",";
    oss << "\"totalAttackers\":" << m_metadata.totalAttackers;
    oss << "},";

    oss << "\"steps\":[";
    for (size_t i = 0; i < m_steps.size(); ++i) {
        if (i) oss << ",";
        serializeStep(oss, m_steps[i]);
    }
    oss << "]";

    oss << ",\"eventStream\":[";
    for (size_t i = 0; i < m_eventStream.size(); ++i) {
        if (i) oss << ",";
        oss << m_eventStream[i];
    }
    oss << "]";

    oss << "}";

    ofs << oss.str();
    ofs.close();
    return true;
}

bool SimulationRecorder::saveBinary(const std::string& filepath) const {
    std::ofstream ofs(filepath, std::ios::binary);
    if (!ofs.is_open()) return false;

    // Header
    struct Header {
        char magic[4] = {'U', 'U', 'V', 'R'};
        uint32_t version = 1;
        uint32_t runId;
        uint32_t maxSteps;
        uint32_t numSteps;
        uint32_t numEvents;
        double noiseLevel;
        unsigned seed;
    } header;

    header.runId = static_cast<uint32_t>(m_metadata.runId);
    header.maxSteps = static_cast<uint32_t>(m_metadata.maxSteps);
    header.numSteps = static_cast<uint32_t>(m_steps.size());
    header.numEvents = static_cast<uint32_t>(m_eventStream.size());
    header.noiseLevel = m_metadata.noiseLevel;
    header.seed = m_metadata.seed;

    ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Steps
    for (const auto& step : m_steps) {
        uint32_t numSeekers = static_cast<uint32_t>(step.seekerX.size());
        uint32_t numTargets = static_cast<uint32_t>(step.targetX.size());
        uint32_t numDetectors = static_cast<uint32_t>(step.detectorX.size());
        uint32_t numInterceptors = static_cast<uint32_t>(step.interceptorX.size());
        uint32_t numAttackers = static_cast<uint32_t>(step.attackerX.size());

        ofs.write(reinterpret_cast<const char*>(&numSeekers), sizeof(uint32_t));
        ofs.write(reinterpret_cast<const char*>(&numTargets), sizeof(uint32_t));
        ofs.write(reinterpret_cast<const char*>(&numDetectors), sizeof(uint32_t));
        ofs.write(reinterpret_cast<const char*>(&numInterceptors), sizeof(uint32_t));
        ofs.write(reinterpret_cast<const char*>(&numAttackers), sizeof(uint32_t));

        for (float v : step.seekerX) ofs.write(reinterpret_cast<const char*>(&v), sizeof(float));
        for (float v : step.seekerY) ofs.write(reinterpret_cast<const char*>(&v), sizeof(float));
        for (bool v : step.seekerAlive) ofs.write(reinterpret_cast<const char*>(&v), sizeof(bool));
        for (bool v : step.seekerDetected) ofs.write(reinterpret_cast<const char*>(&v), sizeof(bool));

        for (float v : step.targetX) ofs.write(reinterpret_cast<const char*>(&v), sizeof(float));
        for (float v : step.targetY) ofs.write(reinterpret_cast<const char*>(&v), sizeof(float));
        for (bool v : step.targetAlive) ofs.write(reinterpret_cast<const char*>(&v), sizeof(bool));

        for (float v : step.detectorX) ofs.write(reinterpret_cast<const char*>(&v), sizeof(float));
        for (float v : step.detectorY) ofs.write(reinterpret_cast<const char*>(&v), sizeof(float));
        for (int v : step.detectorSightings) ofs.write(reinterpret_cast<const char*>(&v), sizeof(int));

        for (float v : step.interceptorX) ofs.write(reinterpret_cast<const char*>(&v), sizeof(float));
        for (float v : step.interceptorY) ofs.write(reinterpret_cast<const char*>(&v), sizeof(float));
        for (int v : step.interceptorEngagements) ofs.write(reinterpret_cast<const char*>(&v), sizeof(int));

        for (float v : step.attackerX) ofs.write(reinterpret_cast<const char*>(&v), sizeof(float));
        for (float v : step.attackerY) ofs.write(reinterpret_cast<const char*>(&v), sizeof(float));
        for (bool v : step.attackerAlive) ofs.write(reinterpret_cast<const char*>(&v), sizeof(bool));
        for (int v : step.attackerState) ofs.write(reinterpret_cast<const char*>(&v), sizeof(int));

        uint32_t numEvents = static_cast<uint32_t>(step.events.size());
        ofs.write(reinterpret_cast<const char*>(&numEvents), sizeof(uint32_t));
        for (int v : step.events) ofs.write(reinterpret_cast<const char*>(&v), sizeof(int));
    }

    ofs.close();
    return true;
}

void SimulationRecorder::clear() noexcept {
    m_steps.clear();
    m_eventStream.clear();
}
