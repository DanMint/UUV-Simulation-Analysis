#ifndef MEDIUM_DETECTOR_AGENT_H
#define MEDIUM_DETECTOR_AGENT_H

#include "../detectorAgent.h"

/**
 * MediumDetectorAgent
 *
 * Improvements over basic:
 *   - 1.25 times the configured sensing radius
 *   - Neutralizes 50% of a seeker's radar-evasion probability
 *   - Cost: 2
 */
struct MediumDetectorAgent : public DetectorAgent {
    MediumDetectorAgent(
        int id,
        int row,
        int col,
        double sensingRadius
    );

    double radarEvasionResistance() const override;
};

#endif // MEDIUM_DETECTOR_AGENT_H