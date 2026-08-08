#ifndef DETECTOR_AGENT_H
#define DETECTOR_AGENT_H

#include <vector>

/**
 * DetectorAgent
 *
 * Stationary sensor on the defender side.
 * SENSE ONLY — detectors do not engage or kill anything.
 *
 * A detector picks up any seeker within its sensing radius and logs a
 * sighting. The seeker becomes "tracked" (sticky: once detected, always
 * detected for the rest of the run) and is then eligible for engagement
 * by an InterceptorAgent (sense-then-shoot doctrine).
 *
 * Persistent and invisible to seekers — seekers do not path around them.
 */
struct DetectorAgent {
    int id;
    int row;
    int col;
    double sensingRadius;   // detection range in cells (Euclidean)
    bool alive;
    int sightingCount;      // total (seeker, step) sightings logged
    int freqLowHz  = 0;       // lower bound of detectable frequency range
    int freqHighHz = 999999;  // upper bound — defaults to detect everything
    float unitCost;         // deployment cost for GA fitness (1-3 scale)

    struct Sighting {
        int seekerId;
        int step;
    };
    std::vector<Sighting> sightings;

    DetectorAgent(int id, int row, int col, double sensingRadius);
    DetectorAgent(int id, int row, int col, double sensingRadius, float unitCost);

    /** True if a position is within this detector's sensing radius. */
    bool isInRange(int checkRow, int checkCol) const;

    /** Log a sighting of a seeker at a given step. */
    void recordSighting(int seekerId, int step);
};

#endif // DETECTOR_AGENT_H