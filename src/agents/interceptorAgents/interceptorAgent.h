#ifndef INTERCEPTOR_AGENT_H
#define INTERCEPTOR_AGENT_H

#include <vector>

/**
 * InterceptorAgent
 *
 * Base class for stationary defender effectors.
 *
 * An interceptor may engage only a seeker that has already been detected by
 * at least one DetectorAgent. This preserves the simulation's
 * sense-then-shoot doctrine.
 *
 * Base engagement model (used by BasicInterceptorAgent):
 *   - inner 50% of kill radius: 90% kill probability
 *   - 50%-70% of kill radius:   60% kill probability
 *   - 70%-100% of kill radius:  50% kill probability
 *
 * Concrete interceptor types may override killProbability() while reusing
 * the shared range checking, state, and intercept logging in this class.
 */
struct InterceptorAgent {
    int id;
    int row;
    int col;

    double killRadius;
    bool alive;

    // Relative acquisition/deployment cost assigned by the concrete type.
    int cost;

    int killCount;

    struct Intercept {
        int seekerId;
        int step;
    };

    std::vector<Intercept> intercepts;

    InterceptorAgent(
        int id,
        int row,
        int col,
        double killRadius
    );

    /** Required when derived interceptors are owned through base pointers. */
    virtual ~InterceptorAgent() = default;

    /** True when a position is inside the interceptor's kill radius. */
    bool isInRange(int checkRow, int checkCol) const;

    /**
     * Return the distance-tiered kill probability at a position.
     *
     * Returns 0.0 when the position is outside killRadius. Derived
     * interceptor classes override this function to provide stronger models.
     */
    virtual double killProbability(
        int checkRow,
        int checkCol
    ) const;

    /** Record a successful intercept. */
    void recordIntercept(int seekerId, int step);
};

#endif // INTERCEPTOR_AGENT_H