#ifndef HUNTER_AGENT_H
#define HUNTER_AGENT_H

#include <vector>
#include "pathfinding.h"

/**
 * HunterAgent
 *
 * Pursuer that pathfinds to the nearest alive seeker and moves toward it.
 * Tracks its movement history for analysis.
 */
struct HunterAgent {
    int id;
    int row;
    int col;
    int spawnRow;
    int spawnCol;
    bool alive;

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

    // Capture state
    bool capturedSeeker;
    int capturedSeekerId;
    int capturedAtStep;

    HunterAgent(int id, int row, int col);

    /** Compute A* path to a seeker. Updates path, pathCost, nodesExpanded. */
    void computePath(const Pathfinding& pf, int destRow, int destCol);

    /** Move one cell along the computed path. Returns true if moved. */
    bool moveStep();

    /** True iff we have a path with remaining waypoints. */
    bool hasPath() const;
};

#endif // HUNTER_AGENT_H
