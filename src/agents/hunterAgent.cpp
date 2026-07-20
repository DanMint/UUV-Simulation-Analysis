#include "hunterAgent.h"

HunterAgent::HunterAgent(int id, int row, int col)
    : id(id), row(row), col(col),
      spawnRow(row), spawnCol(col),
      alive(true),
      pathIndex(0), targetId(-1),
      stepsTaken(0), pathCost(0.0), nodesExpanded(0),
      capturedSeeker(false), capturedSeekerId(-1), capturedAtStep(-1)
{
    moveHistory.push_back({row, col});
}

void HunterAgent::computePath(const Pathfinding& pf, int destRow, int destCol) {
    path = pf.findPath(row, col, destRow, destCol);
    pathIndex = 0;
    pathCost = pf.getLastPathCost();
    nodesExpanded = pf.getLastNodesExpanded();

    if (!path.empty() && path[0].first == row && path[0].second == col) {
        pathIndex = 1;
    }
}

bool HunterAgent::moveStep() {
    if (!alive || path.empty()) return false;
    if (pathIndex >= static_cast<int>(path.size())) return false;

    row = path[pathIndex].first;
    col = path[pathIndex].second;
    pathIndex++;
    stepsTaken++;

    moveHistory.push_back({row, col});
    return true;
}

bool HunterAgent::hasPath() const {
    return !path.empty() && pathIndex < static_cast<int>(path.size());
}
