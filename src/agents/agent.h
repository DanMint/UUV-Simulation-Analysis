#ifndef AGENT_H
#define AGENT_H

#include <vector>
#include <utility>
#include <string>
#include "pathfinding.h"

/**
 * TargetAgent
 *
 * Stationary defender. Sits at a position and waits to be destroyed.
 */
struct TargetAgent {
    int id;
    int row;
    int col;
    bool alive;

    TargetAgent(int id, int row, int col)
        : id(id), row(row), col(col), alive(true) {}
};

/**
 * DetectorAgent
 *
 * Stationary interceptor on the defender side.
 * Destroys any seeker that enters its detection radius.
 * Persistent — can intercept unlimited seekers.
 * Invisible — seekers do NOT path around detectors.
 */
struct DetectorAgent {
    int id;
    int row;
    int col;
    double radius;       // detection radius in cells (Euclidean)
    bool alive;
    int interceptCount;  // how many seekers this detector has destroyed

    // Track which seekers were intercepted and at which step
    struct Intercept {
        int seekerId;
        int step;
    };
    std::vector<Intercept> intercepts;

    DetectorAgent(int id, int row, int col, double radius)
        : id(id), row(row), col(col), radius(radius),
          alive(true), interceptCount(0) {}

    /**
     * Check if a position is within this detector's radius.
     * Uses Euclidean distance.
     */
    bool isInRange(int checkRow, int checkCol) const {
        double dr = row - checkRow;
        double dc = col - checkCol;
        return std::sqrt(dr * dr + dc * dc) <= radius;
    }
};

/**
 * SeekerAgent
 *
 * Attacker that pathfinds to the nearest target and moves toward it.
 * Tracks its full movement history for analysis.
 */
struct SeekerAgent {
    int id;
    int row;           // current position
    int col;
    int spawnRow;      // starting position (for reset between iterations)
    int spawnCol;
    bool alive;
    bool reachedTarget;

    // The path from A* (list of (row,col) waypoints)
    std::vector<Pathfinding::Pos> path;
    int pathIndex;     // which waypoint we're currently moving toward

    // Analysis: full history of positions visited
    std::vector<Pathfinding::Pos> moveHistory;

    // Which target we're currently chasing (-1 if none)
    int targetId;

    // Stats
    int stepsTaken;
    double pathCost;       // A* cost of the computed path
    int nodesExpanded;     // how many nodes A* explored

    // Detector interception tracking
    bool intercepted;      // was this seeker destroyed by a detector?
    int interceptedByDetector;  // which detector killed it (-1 if none)
    int interceptedAtStep;      // at which step (-1 if none)

    SeekerAgent(int id, int row, int col)
        : id(id), row(row), col(col),
          spawnRow(row), spawnCol(col),
          alive(true), reachedTarget(false),
          pathIndex(0), targetId(-1),
          stepsTaken(0), pathCost(0.0), nodesExpanded(0),
          intercepted(false), interceptedByDetector(-1), interceptedAtStep(-1)
    {
        // Record starting position
        moveHistory.push_back({row, col});
    }

    /**
     * Compute path to a target using the given pathfinder.
     * Updates path, pathCost, nodesExpanded.
     */
    void computePath(const Pathfinding& pf, int destRow, int destCol) {
        path = pf.findPath(row, col, destRow, destCol);
        pathIndex = 0;
        pathCost = pf.getLastPathCost();
        nodesExpanded = pf.getLastNodesExpanded();

        // Skip the first waypoint if it's our current position
        if (!path.empty() && path[0].first == row && path[0].second == col) {
            pathIndex = 1;
        }
    }

    /**
     * Move one step along the computed path.
     * Returns true if we moved, false if path is empty or finished.
     */
    bool moveStep() {
        if (!alive || path.empty()) return false;
        if (pathIndex >= static_cast<int>(path.size())) return false;

        row = path[pathIndex].first;
        col = path[pathIndex].second;
        pathIndex++;
        stepsTaken++;

        moveHistory.push_back({row, col});
        return true;
    }

    /** Check if we have a valid path with remaining waypoints. */
    bool hasPath() const {
        return !path.empty() && pathIndex < static_cast<int>(path.size());
    }
};

#endif // AGENT_H