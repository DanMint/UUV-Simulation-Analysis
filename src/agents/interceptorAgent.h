#ifndef INTERCEPTOR_AGENT_H
#define INTERCEPTOR_AGENT_H

#include <vector>
#include <string>

/**
 * InterceptorAgent
 *
 * Stationary effector on the defender side.
 * KILLS tracked seekers only — an interceptor will NOT fire on a seeker
 * unless that seeker has been detected by at least one DetectorAgent
 * (sense-then-shoot doctrine).
 *
 * Engagement model (probabilistic, distance-tiered):
 *   - For a tracked seeker within killRadius, compute ratio = dist / radius.
 *   - Roll a uniform [0,1); kill if roll < kill probability:
 *       inner 50% of radius (ratio <= 0.5)  → 90% kill chance
 *       50%-70% of radius   (ratio <= 0.7)  → 60% kill chance
 *       70%-100% of radius                  → 50% kill chance
 *
 * Persistent — can engage unlimited seekers. Invisible to seekers
 * (they do not path around interceptors).
 */
struct InterceptorAgent {
    int id;
    int row;
    int col;
    double killRadius;     // engagement range in cells (Euclidean)
    bool alive;
    int killCount; 
    std::string droneType;  // total successful kills

    struct Intercept {
        int seekerId;
        int step;
    };
    std::vector<Intercept> intercepts;

    InterceptorAgent(int id, int row, int col, double killRadius, std::string droneType = "unknown");

    /** True if a position is within this interceptor's kill radius. */
    bool isInRange(int checkRow, int checkCol) const;

    /**
     * Distance-tiered kill probability at the given position.
     * Returns 0.0 if the target is outside killRadius.
     */
    double killProbability(int checkRow, int checkCol) const;

    /** Log a successful intercept of a seeker at a given step. */
    void recordIntercept(int seekerId, int step);
};

#endif // INTERCEPTOR_AGENT_H