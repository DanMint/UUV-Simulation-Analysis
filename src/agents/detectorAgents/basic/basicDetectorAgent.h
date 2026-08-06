#ifndef BASIC_DETECTOR_AGENT_H
#define BASIC_DETECTOR_AGENT_H

#include "../detectorAgent.h"

/**
 * BasicDetectorAgent
 *
 * Uses DetectorAgent's sensing radius and radar-evasion resistance unchanged.
 * Cost: 1.
 */
struct BasicDetectorAgent : public DetectorAgent {
    BasicDetectorAgent(
        int id,
        int row,
        int col,
        double sensingRadius
    );
};

#endif // BASIC_DETECTOR_AGENT_H