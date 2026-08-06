#ifndef ADVANCED_DETECTOR_AGENT_H
#define ADVANCED_DETECTOR_AGENT_H

#include "../detectorAgent.h"

/**
 * AdvancedDetectorAgent
 *
 * Improvements over basic:
 *   - 1.50 times the configured sensing radius
 *   - Neutralizes 80% of a seeker's radar-evasion probability
 *   - Cost: 3
 */
struct AdvancedDetectorAgent : public DetectorAgent {
    AdvancedDetectorAgent(
        int id,
        int row,
        int col,
        double sensingRadius
    );

    double radarEvasionResistance() const override;
};

#endif // ADVANCED_DETECTOR_AGENT_H