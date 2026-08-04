#ifndef SEEKER_AGENT_H
#define SEEKER_AGENT_H

#include <vector>
#include "pathfinding.h"

/**
 * SeekerAgent
 *
 * Base class for all seeker implementations.
 *
 * Attacker that pathfinds to the nearest target and moves toward it.
 * Tracks its full movement history for analysis.
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

    // Full move history
    std::vector<Pathfinding::Pos> moveHistory;

    // Current target assignment
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

    // Required when deleting a derived seeker through SeekerAgent*.
    virtual ~SeekerAgent() = default;

    void computePath(const Pathfinding& pf, int destRow, int destCol);

    // Virtual so future seeker types can override movement behavior.
    // BasicSeekerAgent currently inherits this implementation unchanged.
    virtual bool moveStep();

    bool hasPath() const;
};

#endif // SEEKER_AGENT_H