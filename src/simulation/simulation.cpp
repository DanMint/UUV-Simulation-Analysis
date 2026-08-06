#include "simulation.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

/**
 * Factory for seeker implementations.
 *
 * This function is the explicit link between the SpawnConfig type string and
 * the concrete C++ class:
 *
 *   "basic"  -> BasicSeekerAgent
 *   "fast"   -> FastSeekerAgent
 *   "evader" -> EvaderSeekerAgent
 */
std::unique_ptr<SeekerAgent> createSeekerAgent(
    const UnitSpawn& unit,
    int id
) {
    if (unit.category != "seeker") {
        throw std::invalid_argument(
            "createSeekerAgent expected category 'seeker', received: " +
            unit.category
        );
    }

    if (unit.type == "basic") {
        return std::make_unique<BasicSeekerAgent>(
            id,
            unit.row,
            unit.col
        );
    }

    if (unit.type == "fast") {
        return std::make_unique<FastSeekerAgent>(
            id,
            unit.row,
            unit.col
        );
    }

    if (unit.type == "evader") {
        return std::make_unique<EvaderSeekerAgent>(
            id,
            unit.row,
            unit.col
        );
    }

    throw std::invalid_argument(
        "Unsupported seeker type: " + unit.type
    );
}


/**
 * Factory for detector implementations.
 *
 *   "basic"    -> BasicDetectorAgent
 *   "medium"   -> MediumDetectorAgent
 *   "advanced" -> AdvancedDetectorAgent
 */
std::unique_ptr<DetectorAgent> createDetectorAgent(
    const UnitSpawn& unit,
    int id,
    double sensingRadius
) {
    if (unit.category != "detector") {
        throw std::invalid_argument(
            "createDetectorAgent expected category 'detector', received: " +
            unit.category
        );
    }

    if (unit.type == "basic") {
        return std::make_unique<BasicDetectorAgent>(
            id,
            unit.row,
            unit.col,
            sensingRadius
        );
    }

    if (unit.type == "medium") {
        return std::make_unique<MediumDetectorAgent>(
            id,
            unit.row,
            unit.col,
            sensingRadius
        );
    }

    if (unit.type == "advanced") {
        return std::make_unique<AdvancedDetectorAgent>(
            id,
            unit.row,
            unit.col,
            sensingRadius
        );
    }

    throw std::invalid_argument(
        "Unsupported detector type: " + unit.type
    );
}


/**
 * Factory for interceptor implementations.
 *
 *   "basic"    -> BasicInterceptorAgent
 *   "medium"   -> MediumInterceptorAgent
 *   "advanced" -> AdvancedInterceptorAgent
 */
std::unique_ptr<InterceptorAgent> createInterceptorAgent(
    const UnitSpawn& unit,
    int id,
    double killRadius
) {
    if (unit.category != "interceptor") {
        throw std::invalid_argument(
            "createInterceptorAgent expected category 'interceptor', received: " +
            unit.category
        );
    }

    if (unit.type == "basic") {
        return std::make_unique<BasicInterceptorAgent>(
            id,
            unit.row,
            unit.col,
            killRadius
        );
    }

    if (unit.type == "medium") {
        return std::make_unique<MediumInterceptorAgent>(
            id,
            unit.row,
            unit.col,
            killRadius
        );
    }

    if (unit.type == "advanced") {
        return std::make_unique<AdvancedInterceptorAgent>(
            id,
            unit.row,
            unit.col,
            killRadius
        );
    }

    throw std::invalid_argument(
        "Unsupported interceptor type: " + unit.type
    );
}

} // namespace

Simulation::Simulation(
    MapCreation& map,
    const SpawnConfig& config,
    int maxSteps
)
    : m_map(map),
      m_maxSteps(maxSteps),
      m_maxNoiseLevel(config.getMaxNoiseLevel()),
      m_rng(std::random_device{}())
{
    int seekerId = 0;
    int targetId = 0;
    int detectorId = 0;
    int interceptorId = 0;

    const double detectorRadius = config.getDetectorRadius();
    const double interceptorRadius = config.getInterceptorRadius();

    for (const auto& unit : config.getUnits()) {
        /*
         * Category chooses the agent family.
         * Type chooses the implementation within that family.
         */
        if (unit.category == "seeker") {
            m_seekers.push_back(
                createSeekerAgent(unit, seekerId++)
            );
            m_seekerTypes.push_back(unit.type);
        }

        else if (unit.category == "target") {
            if (unit.type != "basic") {
                throw std::invalid_argument(
                    "Unsupported target type: " + unit.type
                );
            }

            m_targets.emplace_back(
                targetId++,
                unit.row,
                unit.col
            );

            m_targetTypes.push_back(unit.type);
        }

        else if (unit.category == "detector") {
            m_detectors.push_back(
                createDetectorAgent(
                    unit,
                    detectorId++,
                    detectorRadius
                )
            );

            m_detectorTypes.push_back(unit.type);
        }

        else if (unit.category == "interceptor") {
            m_interceptors.push_back(
                createInterceptorAgent(
                    unit,
                    interceptorId++,
                    interceptorRadius
                )
            );

            m_interceptorTypes.push_back(unit.type);
        }

        else {
            throw std::invalid_argument(
                "Unknown unit category: " + unit.category
            );
        }
    }

    std::cout
        << "Simulation created: "
        << m_seekers.size() << " seekers, "
        << m_targets.size() << " targets, "
        << m_detectors.size()
        << " detectors (r=" << detectorRadius << "), "
        << m_interceptors.size()
        << " interceptors (r=" << interceptorRadius << "), "
        << "noise=" << m_maxNoiseLevel << ", "
        << "max " << m_maxSteps << " steps\n";

    if (!m_detectors.empty() && m_interceptors.empty()) {
        std::cout
            << "  WARNING: detectors present but no interceptors. "
            << "Seekers will be tracked but never killed.\n";
    }

    if (m_detectors.empty() && !m_interceptors.empty()) {
        std::cout
            << "  WARNING: interceptors present but no detectors. "
            << "Under sense-then-shoot doctrine, no seeker can be tracked, "
            << "so interceptors will never fire.\n";
    }
}

int Simulation::findNearestTarget(
    const SeekerAgent& seeker
) const {
    int bestIndex = -1;
    double bestDistance = std::numeric_limits<double>::max();

    for (int index = 0;
         index < static_cast<int>(m_targets.size());
         ++index) {

        if (!m_targets[index].alive) {
            continue;
        }

        const double rowDifference =
            seeker.row - m_targets[index].row;

        const double columnDifference =
            seeker.col - m_targets[index].col;

        const double distance = std::sqrt(
            rowDifference * rowDifference +
            columnDifference * columnDifference
        );

        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = index;
        }
    }

    return bestIndex;
}

bool Simulation::checkCollision(
    const SeekerAgent& seeker,
    const TargetAgent& target
) const {
    return seeker.row == target.row &&
           seeker.col == target.col;
}

// ════════════════════════════════════════════════════════════════════════════════
//  SENSE PHASE — DETECTORS UPDATE TRACKS
// ════════════════════════════════════════════════════════════════════════════════

void Simulation::updateDetectorTracks(int currentStep) {
    for (auto& seekerPointer : m_seekers) {
        SeekerAgent& seeker = *seekerPointer;

        if (!seeker.alive || seeker.reachedTarget) {
            continue;
        }

        for (auto& detectorPointer : m_detectors) {
            DetectorAgent& detector = *detectorPointer;

            if (!detector.alive) {
                continue;
            }

            if (!detector.isInRange(seeker.row, seeker.col)) {
                continue;
            }

            const double seekerRadarEvasion = std::clamp(
                seeker.radarEvasionProbability(),
                0.0,
                1.0
            );

            const double detectorResistance = std::clamp(
                detector.radarEvasionResistance(),
                0.0,
                1.0
            );

            // The detector removes a fraction of the seeker's evasion ability.
            const double effectiveEvasion =
                seekerRadarEvasion * (1.0 - detectorResistance);

            const double probability = 1.0 - effectiveEvasion;

            std::bernoulli_distribution detectionRoll(probability);

            if (!detectionRoll(m_rng)) {
                std::cout
                    << "  Step " << currentStep
                    << ": Seeker " << seeker.id
                    << " evaded Detector " << detector.id
                    << " at (" << seeker.row
                    << "," << seeker.col << ")"
                    << " [pDetect=" << std::fixed
                    << std::setprecision(1)
                    << probability * 100.0 << "%]\n";
                continue;
            }

            detector.recordSighting(
                seeker.id,
                currentStep
            );

            if (!seeker.detected) {
                seeker.detected = true;
                seeker.firstDetectedAtStep = currentStep;
                seeker.firstDetectedByDetector = detector.id;

                std::cout
                    << "  Step " << currentStep
                    << ": Detector " << detector.id
                    << " acquired Seeker " << seeker.id
                    << " at (" << seeker.row
                    << "," << seeker.col << ")"
                    << " [pDetect=" << std::fixed
                    << std::setprecision(1)
                    << probability * 100.0 << "%]\n";
            }
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════════
//  SHOOT PHASE — INTERCEPTORS ENGAGE TRACKED SEEKERS
// ════════════════════════════════════════════════════════════════════════════════

void Simulation::checkInterceptorEngagements(
    int currentStep
) {
    std::uniform_real_distribution<double> roll(0.0, 1.0);

    for (auto& seekerPointer : m_seekers) {
        SeekerAgent& seeker = *seekerPointer;

        if (!seeker.alive || seeker.reachedTarget) {
            continue;
        }

        if (!seeker.detected) {
            continue;
        }

        for (auto& interceptorPointer : m_interceptors) {
            InterceptorAgent& interceptor = *interceptorPointer;

            if (!interceptor.alive) {
                continue;
            }

            if (!interceptor.isInRange(
                    seeker.row,
                    seeker.col
                )) {
                continue;
            }

            const double interceptorEvasion =
                seeker.interceptorEvasionProbability();

            if (interceptorEvasion > 0.0) {
                std::bernoulli_distribution evadeInterceptor(
                    interceptorEvasion
                );

                if (evadeInterceptor(m_rng)) {
                    std::cout
                        << "  Step " << currentStep
                        << ": Seeker " << seeker.id
                        << " evaded Interceptor " << interceptor.id
                        << " at (" << seeker.row
                        << "," << seeker.col << ")\n";
                    continue;
                }
            }

            const double killProbability =
                interceptor.killProbability(
                    seeker.row,
                    seeker.col
                );

            const double randomRoll = roll(m_rng);

            const double rowDifference =
                interceptor.row - seeker.row;

            const double columnDifference =
                interceptor.col - seeker.col;

            const double distance = std::sqrt(
                rowDifference * rowDifference +
                columnDifference * columnDifference
            );

            const double radiusRatio =
                interceptor.killRadius > 0.0
                    ? distance / interceptor.killRadius
                    : 0.0;

            if (randomRoll < killProbability) {
                std::cout
                    << "  Step " << currentStep
                    << ": Interceptor " << interceptor.id
                    << " killed Seeker " << seeker.id
                    << " at (" << seeker.row
                    << "," << seeker.col << ")"
                    << " [dist=" << std::fixed
                    << std::setprecision(1)
                    << (radiusRatio * 100)
                    << "%, p=" << (killProbability * 100)
                    << "%]\n";

                seeker.alive = false;
                seeker.intercepted = true;
                seeker.interceptedByInterceptor =
                    interceptor.id;
                seeker.interceptedAtStep = currentStep;

                interceptor.recordIntercept(
                    seeker.id,
                    currentStep
                );

                break;
            }

            std::cout
                << "  Step " << currentStep
                << ": Interceptor " << interceptor.id
                << " missed Seeker " << seeker.id
                << " [dist=" << std::fixed
                << std::setprecision(1)
                << (radiusRatio * 100)
                << "%, p=" << (killProbability * 100)
                << "%]\n";
        }
    }
}

bool Simulation::applyNoise(SeekerAgent& seeker) {
    if (m_maxNoiseLevel <= 0.0) {
        return false;
    }

    if (!seeker.alive || seeker.reachedTarget) {
        return false;
    }

    std::uniform_real_distribution<double> displacement(
        -m_maxNoiseLevel,
        m_maxNoiseLevel
    );

    const int columnOffset =
        static_cast<int>(std::round(displacement(m_rng)));

    const int rowOffset =
        static_cast<int>(std::round(displacement(m_rng)));

    if (columnOffset == 0 && rowOffset == 0) {
        return false;
    }

    const int newRow = seeker.row + rowOffset;
    const int newColumn = seeker.col + columnOffset;

    if (!m_map.isValid(newRow, newColumn)) {
        return false;
    }

    if (!m_map.isPassable(newRow, newColumn)) {
        return false;
    }

    // Bresenham line-of-sight check.
    {
        const int destinationRow = newRow;
        const int destinationColumn = newColumn;

        int currentRow = seeker.row;
        int currentColumn = seeker.col;

        const int rowDistance =
            std::abs(destinationRow - currentRow);

        const int columnDistance =
            std::abs(destinationColumn - currentColumn);

        const int rowDirection =
            currentRow < destinationRow ? 1 : -1;

        const int columnDirection =
            currentColumn < destinationColumn ? 1 : -1;

        int error = columnDistance - rowDistance;

        while (currentRow != destinationRow ||
               currentColumn != destinationColumn) {

            const int doubledError = 2 * error;

            if (doubledError > -rowDistance) {
                error -= rowDistance;
                currentColumn += columnDirection;
            }

            if (doubledError < columnDistance) {
                error += columnDistance;
                currentRow += rowDirection;
            }

            if (!m_map.isValid(
                    currentRow,
                    currentColumn
                ) ||
                !m_map.isPassable(
                    currentRow,
                    currentColumn
                )) {
                return false;
            }
        }
    }

    seeker.row = newRow;
    seeker.col = newColumn;

    seeker.moveHistory.push_back({
        newRow,
        newColumn
    });

    /*
     * The current A* path starts from the old position and is no longer valid.
     * assignTargets() will compute a replacement path later in this step.
     */
    seeker.path.clear();
    seeker.pathIndex = 0;

    return true;
}

void Simulation::assignTargets(
    const Pathfinding& pathfinding
) {
    std::vector<bool> targetAssigned(
        m_targets.size(),
        false
    );

    for (auto& seekerPointer : m_seekers) {
        SeekerAgent& seeker = *seekerPointer;

        if (!seeker.alive || seeker.reachedTarget) {
            continue;
        }

        int bestIndex = -1;
        double bestDistance =
            std::numeric_limits<double>::max();

        /*
         * Prefer an unclaimed target. If all living targets are already
         * assigned, fall back to the nearest living target.
         */
        for (int index = 0;
             index < static_cast<int>(m_targets.size());
             ++index) {

            if (!m_targets[index].alive ||
                targetAssigned[index]) {
                continue;
            }

            const double rowDifference =
                seeker.row - m_targets[index].row;

            const double columnDifference =
                seeker.col - m_targets[index].col;

            const double distance = std::sqrt(
                rowDifference * rowDifference +
                columnDifference * columnDifference
            );

            if (distance < bestDistance) {
                bestDistance = distance;
                bestIndex = index;
            }
        }

        if (bestIndex < 0) {
            for (int index = 0;
                 index < static_cast<int>(m_targets.size());
                 ++index) {

                if (!m_targets[index].alive) {
                    continue;
                }

                const double rowDifference =
                    seeker.row - m_targets[index].row;

                const double columnDifference =
                    seeker.col - m_targets[index].col;

                const double distance = std::sqrt(
                    rowDifference * rowDifference +
                    columnDifference * columnDifference
                );

                if (distance < bestDistance) {
                    bestDistance = distance;
                    bestIndex = index;
                }
            }
        }

        if (bestIndex < 0) {
            continue;
        }

        targetAssigned[bestIndex] = true;

        if (seeker.targetId != m_targets[bestIndex].id ||
            !seeker.hasPath()) {

            seeker.targetId = m_targets[bestIndex].id;

            seeker.computePath(
                pathfinding,
                m_targets[bestIndex].row,
                m_targets[bestIndex].col
            );

            if (seeker.path.empty()) {
                std::cout
                    << "  Seeker " << seeker.id
                    << ": no path to target "
                    << bestIndex << "\n";
            }
        }
    }
}

SimResult Simulation::buildResult(
    int totalSteps
) const {
    SimResult result;

    result.totalSteps = totalSteps;
    result.allTargetsDestroyed = true;
    result.allSeekersDead = true;
    result.maxNoiseLevel = m_maxNoiseLevel;

    // ── Seekers ─────────────────────────────────────────────────────

    for (std::size_t index = 0;
         index < m_seekers.size();
         ++index) {
        const SeekerAgent& seeker = *m_seekers[index];

        SimResult::SeekerResult seekerResult;

        seekerResult.id = seeker.id;
        seekerResult.category = "seeker";
        seekerResult.type = m_seekerTypes.at(index);
        seekerResult.cost = seeker.cost;

        seekerResult.stepsTaken = seeker.stepsTaken;
        seekerResult.pathCost = seeker.pathCost;
        seekerResult.nodesExpanded =
            seeker.nodesExpanded;
        seekerResult.reachedTarget =
            seeker.reachedTarget;
        seekerResult.targetId = seeker.targetId;
        seekerResult.moveHistory =
            seeker.moveHistory;

        seekerResult.detected = seeker.detected;
        seekerResult.firstDetectedAtStep =
            seeker.firstDetectedAtStep;
        seekerResult.firstDetectedByDetector =
            seeker.firstDetectedByDetector;

        seekerResult.intercepted =
            seeker.intercepted;
        seekerResult.interceptedByInterceptor =
            seeker.interceptedByInterceptor;
        seekerResult.interceptedAtStep =
            seeker.interceptedAtStep;

        result.seekerResults.push_back(
            std::move(seekerResult)
        );

        if (seeker.alive) {
            result.allSeekersDead = false;
        }
    }

    // ── Targets ─────────────────────────────────────────────────────

    for (std::size_t index = 0;
         index < m_targets.size();
         ++index) {

        const TargetAgent& target = m_targets[index];

        SimResult::TargetResult targetResult;

        targetResult.id = target.id;
        targetResult.category = "target";
        targetResult.type = m_targetTypes.at(index);
        targetResult.row = target.row;
        targetResult.col = target.col;
        targetResult.destroyed = !target.alive;
        targetResult.destroyedAtStep = -1;
        targetResult.destroyedBySeeker = -1;

        result.targetResults.push_back(
            std::move(targetResult)
        );

        if (target.alive) {
            result.allTargetsDestroyed = false;
        }
    }

    // ── Detectors ───────────────────────────────────────────────────

    for (std::size_t index = 0;
         index < m_detectors.size();
         ++index) {

        const DetectorAgent& detector =
            *m_detectors[index];

        SimResult::DetectorResult detectorResult;

        detectorResult.id = detector.id;
        detectorResult.category = "detector";
        detectorResult.type =
            m_detectorTypes.at(index);
        detectorResult.cost = detector.cost;
        detectorResult.row = detector.row;
        detectorResult.col = detector.col;
        detectorResult.sensingRadius =
            detector.sensingRadius;
        detectorResult.sightingCount =
            detector.sightingCount;

        for (const auto& sighting :
             detector.sightings) {

            detectorResult.sightings.push_back({
                sighting.seekerId,
                sighting.step
            });
        }

        result.detectorResults.push_back(
            std::move(detectorResult)
        );
    }

    // ── Interceptors ────────────────────────────────────────────────

    for (std::size_t index = 0;
         index < m_interceptors.size();
         ++index) {

        const InterceptorAgent& interceptor =
            *m_interceptors[index];

        SimResult::InterceptorResult interceptorResult;

        interceptorResult.id = interceptor.id;
        interceptorResult.category = "interceptor";
        interceptorResult.type =
            m_interceptorTypes.at(index);
        interceptorResult.cost = interceptor.cost;
        interceptorResult.row = interceptor.row;
        interceptorResult.col = interceptor.col;
        interceptorResult.killRadius =
            interceptor.killRadius;
        interceptorResult.killCount =
            interceptor.killCount;

        for (const auto& intercept :
             interceptor.intercepts) {

            interceptorResult.intercepts.push_back({
                intercept.seekerId,
                intercept.step
            });
        }

        result.interceptorResults.push_back(
            std::move(interceptorResult)
        );
    }

    result.computeSummary();
    return result;
}

SimResult Simulation::run() {
    std::cout << "\n--- Simulation starting ---\n";

    Pathfinding pathfinding(m_map.getGrid());
    assignTargets(pathfinding);

    int step = 0;
    bool allTargetsDead = false;
    bool allSeekersFinished = false;

    while (step < m_maxSteps &&
           !allTargetsDead &&
           !allSeekersFinished) {

        ++step;

        // ── 1. Type-specific seeker movement ───────────────────────

        for (auto& seekerPointer : m_seekers) {
            SeekerAgent& seeker = *seekerPointer;

            if (!seeker.alive ||
                seeker.reachedTarget) {
                continue;
            }

            /*
             * BasicSeekerAgent currently inherits SeekerAgent::moveStep().
             * Future seeker classes can override the virtual function.
             */
            seeker.moveStep();
        }

        // ── 2. Environmental noise ──────────────────────────────────

        if (m_maxNoiseLevel > 0.0) {
            for (auto& seekerPointer : m_seekers) {
                applyNoise(*seekerPointer);
            }
        }

        // ── 3. SENSE ────────────────────────────────────────────────

        updateDetectorTracks(step);

        // ── 4. SHOOT ────────────────────────────────────────────────

        checkInterceptorEngagements(step);

        // ── 5. Seeker-target collisions ─────────────────────────────

        for (auto& seekerPointer : m_seekers) {
            SeekerAgent& seeker = *seekerPointer;

            if (!seeker.alive ||
                seeker.reachedTarget) {
                continue;
            }

            for (auto& target : m_targets) {
                if (!target.alive) {
                    continue;
                }

                if (!checkCollision(
                        seeker,
                        target
                    )) {
                    continue;
                }

                std::cout
                    << "  Step " << step
                    << ": Seeker " << seeker.id
                    << " reached Target " << target.id
                    << " at (" << target.row
                    << "," << target.col << ")\n";

                target.alive = false;
                seeker.reachedTarget = true;
                break;
            }
        }

        // ── 6. Retarget if needed ───────────────────────────────────

        bool needsRetarget = false;

        for (const auto& seekerPointer : m_seekers) {
            const SeekerAgent& seeker =
                *seekerPointer;

            if (!seeker.alive ||
                seeker.reachedTarget) {
                continue;
            }

            if (seeker.targetId >= 0 &&
                seeker.targetId <
                    static_cast<int>(m_targets.size()) &&
                !m_targets[seeker.targetId].alive) {

                needsRetarget = true;
                break;
            }

            if (!seeker.hasPath()) {
                needsRetarget = true;
                break;
            }
        }

        if (needsRetarget) {
            assignTargets(pathfinding);
        }

        // ── 7. Termination ──────────────────────────────────────────

        allTargetsDead = true;

        for (const auto& target : m_targets) {
            if (target.alive) {
                allTargetsDead = false;
                break;
            }
        }

        allSeekersFinished = true;

        for (const auto& seekerPointer : m_seekers) {
            const SeekerAgent& seeker =
                *seekerPointer;

            if (seeker.alive &&
                !seeker.reachedTarget &&
                seeker.hasPath()) {

                allSeekersFinished = false;
                break;
            }
        }
    }

    std::cout
        << "--- Simulation finished at step "
        << step << " ---\n";

    SimResult result = buildResult(step);

    /*
     * Patch target destruction information using the seeker that reached it.
     */
    for (auto& targetResult : result.targetResults) {
        if (!targetResult.destroyed) {
            continue;
        }

        for (const auto& seekerResult :
             result.seekerResults) {

            if (seekerResult.reachedTarget &&
                seekerResult.targetId ==
                    targetResult.id) {

                targetResult.destroyedBySeeker =
                    seekerResult.id;

                targetResult.destroyedAtStep =
                    seekerResult.stepsTaken;

                break;
            }
        }
    }

    result.computeSummary();
    return result;
}