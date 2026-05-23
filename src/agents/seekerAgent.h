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
 * Detection lifecycle (sense-then-shoot):
 *   1. `detected` starts false.
 *   2. When a DetectorAgent first picks the seeker up:
 *        - `detected` becomes true (sticky — stays true for the run)
 *        - `firstDetectedAtStep` and `firstDetectedByDetector` are set
 *   3. InterceptorAgents can only engage seekers with `detected == true`.
 *
 * Interception lifecycle:
 *   - When an interceptor successfully kills the seeker:
 *        - `alive` becomes false, `intercepted` becomes true
 *        - `interceptedByInterceptor` and `interceptedAtStep` are set
 */
struct SeekerAgent {
    int id;
    int row;
    int col;
    int spawnRow;
    int spawnCol;
    bool alive;
    bool reachedTarget;

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

    // ── Detection state (written by DetectorAgents) ─────────────────
    bool detected;                  // tracked by any detector yet?
    int firstDetectedAtStep;        // step of first detection (-1 if never)
    int firstDetectedByDetector;    // id of first detector to track it (-1)

    // ── Interception state (written by InterceptorAgents) ───────────
    bool intercepted;               // killed by an interceptor?
    int interceptedByInterceptor;   // id of the killing interceptor (-1)
    int interceptedAtStep;          // step of the kill (-1)

    SeekerAgent(int id, int row, int col);

    /** Compute A* path to a target. Updates path, pathCost, nodesExpanded. */
    void computePath(const Pathfinding& pf, int destRow, int destCol);

    /** Move one cell along the computed path. Returns true if moved. */
    bool moveStep();

    /** True iff we have a path with remaining waypoints. */
    bool hasPath() const;
};

#endif // SEEKER_AGENT_H