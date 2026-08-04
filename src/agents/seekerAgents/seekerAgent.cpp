#include "seekerAgent.h"

SeekerAgent::SeekerAgent(int id, int row, int col)
    : id(id), row(row), col(col),
      spawnRow(row), spawnCol(col),
      alive(true), reachedTarget(false),
      pathIndex(0), targetId(-1),
      stepsTaken(0), pathCost(0.0), nodesExpanded(0),
      detected(false),
      firstDetectedAtStep(-1),
      firstDetectedByDetector(-1),
      intercepted(false),
      interceptedByInterceptor(-1),
      interceptedAtStep(-1),
      cost(0)
{
    // Record starting position
    moveHistory.push_back({row, col});
}

void SeekerAgent::computePath(const Pathfinding& pf, int destRow, int destCol) {
    path = pf.findPath(row, col, destRow, destCol);
    pathIndex = 0;
    pathCost = pf.getLastPathCost();
    nodesExpanded = pf.getLastNodesExpanded();

    // Skip the first waypoint if it's our current position
    if (!path.empty() && path[0].first == row && path[0].second == col) {
        pathIndex = 1;
    }
}

bool SeekerAgent::moveStep() {
    if (!alive || path.empty()) return false;
    if (pathIndex >= static_cast<int>(path.size())) return false;

    row = path[pathIndex].first;
    col = path[pathIndex].second;
    pathIndex++;
    stepsTaken++;

    moveHistory.push_back({row, col});
    return true;
}

bool SeekerAgent::hasPath() const {
    return !path.empty() && pathIndex < static_cast<int>(path.size());
}