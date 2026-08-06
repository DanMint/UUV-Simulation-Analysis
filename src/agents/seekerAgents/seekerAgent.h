#ifndef SEEKER_AGENT_H
#define SEEKER_AGENT_H

#include <vector>
#include "pathfinding.h"

/**
 * SeekerAgent
 *
 * Attacker that pathfinds to the nearest target and moves toward it.
 * Tracks its full movement history for analysis.
 *
 * This is the base class for concrete seeker types such as:
 *   - BasicSeekerAgent
 *   - FastSeekerAgent
 *   - EvaderSeekerAgent
 */
struct SeekerAgent {
    int id;
    int row;
    int col;
    int spawnRow;
    int spawnCol;
    bool alive;
    bool reachedTarget;
    int cost;

    // A* path
    std::vector<Pathfinding::Pos> path;
    int pathIndex;

    // Full move history (every position visited)
    std::vector<Pathfinding::Pos> moveHistory;

    // Current target assignment (-1 if none)
    int targetId;

    // Stats
    int stepsTaken;
    double pathCost;
    int nodesExpanded;

    // Detection state
    bool detected;
    int firstDetectedAtStep;
    int firstDetectedByDetector;

    // Interception state
    bool intercepted;
    int interceptedByInterceptor;
    int interceptedAtStep;

    SeekerAgent(int id, int row, int col);

    /** Required when deleting derived seekers through SeekerAgent pointers. */
    virtual ~SeekerAgent() = default;

    /** Compute A* path to a target. Updates path, pathCost, nodesExpanded. */
    void computePath(const Pathfinding& pf, int destRow, int destCol);

    /** Move along the computed path. Concrete types may override this. */
    virtual bool moveStep();

    /** Probability of evading one detector observation attempt. */
    virtual double radarEvasionProbability() const;

    /** Probability of evading one interceptor engagement attempt. */
    virtual double interceptorEvasionProbability() const;

    /** True iff we have a path with remaining waypoints. */
    bool hasPath() const;
};

#endif // SEEKER_AGENT_H