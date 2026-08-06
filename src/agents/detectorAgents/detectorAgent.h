#ifndef DETECTOR_AGENT_H
#define DETECTOR_AGENT_H

#include <vector>

/**
 * DetectorAgent
 *
 * Base class for stationary defender-side sensors.
 *
 * SENSE ONLY — detectors do not engage or kill anything.
 * A detector acquires seekers inside its sensing radius and records sightings.
 * Concrete detector types may provide a larger sensing radius and may reduce a
 * seeker's radar-evasion probability.
 */
struct DetectorAgent {
    int id;
    int row;
    int col;

    /** Effective detection range in grid cells. */
    double sensingRadius;

    bool alive;
    int sightingCount;

    /** Resource cost used by scenario/GA budget calculations. */
    int cost;

    struct Sighting {
        int seekerId;
        int step;
    };

    std::vector<Sighting> sightings;

    DetectorAgent(int id, int row, int col, double sensingRadius);

    /** Required when derived detectors are owned through DetectorAgent pointers. */
    virtual ~DetectorAgent() = default;

    /** True when a position is inside this detector's sensing radius. */
    bool isInRange(int checkRow, int checkCol) const;

    /** Record one successful detector sighting. */
    void recordSighting(int seekerId, int step);

    /**
     * Fraction of seeker radar-evasion capability neutralized by this detector.
     *
     * 0.00 means no resistance to evasion.
     * 0.50 means 50% of the seeker's evasion probability is removed.
     * 0.80 means 80% of the seeker's evasion probability is removed.
     */
    virtual double radarEvasionResistance() const;
};

#endif // DETECTOR_AGENT_H