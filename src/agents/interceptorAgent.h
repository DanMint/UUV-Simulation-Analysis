#ifndef INTERCEPTOR_AGENT_H
#define INTERCEPTOR_AGENT_H

#include <string>
#include <vector>

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
 *
 * Cost model (Lance's framing): the cost of DEFENCE is the cost of what
 * is actually EXPENDED — namely the interceptor's shots. Every shot
 * costs money, hit or miss. A premium interceptor (e.g. HUGIN at $2-4M)
 * fires expensive munitions, while a budget interceptor (BlueROV2 at
 * $6k) fires cheap ones — so the "$2M interceptor vs $1000 drone" trade
 * is captured in the per-shot cost.
 */
struct InterceptorAgent {
    int id;
    int row;
    int col;
    double killRadius;     // engagement range in cells (Euclidean)
    bool alive;
    int killCount;         // total successful kills
    int engagementCount;   // total shots fired (hits + misses)
    float engagementCost;  // cost per shot ($)
    float unitCost;        // deployment cost for GA fitness (1-3 scale)
    std::string vehicleType;  // optional vehicle model (e.g. "hugin", "yuco")
    static constexpr float DEFAULT_COST_PER_SHOT = 50000.0f;  // $50k default per shot

    struct Intercept {
        int seekerId;
        int step;
    };
    std::vector<Intercept> intercepts;

    InterceptorAgent(int id, int row, int col, double killRadius);
    InterceptorAgent(int id, int row, int col, double killRadius,
                     const std::string& vehicleType);
    InterceptorAgent(int id, int row, int col, double killRadius,
                     const std::string& vehicleType, float unitCost, float engagementCost);

    /**
     * Compute the per-shot cost for a given interceptor vehicle type.
     * Derives a realistic munition cost from the vehicle's unit cost
     * (a premium platform fires more expensive ordnance). Falls back to
     * DEFAULT_COST_PER_SHOT for unknown/generic types.
     */
    static float costPerShotForType(const std::string& vehicleType);

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
