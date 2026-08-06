#include "fastSeekerAgent.h"

FastSeekerAgent::FastSeekerAgent(int id, int row, int col)
    : SeekerAgent(id, row, col)
{
    this->cost = 2;
}

bool FastSeekerAgent::moveStep() {
    if (!alive || path.empty()) return false;
    if (pathIndex >= static_cast<int>(path.size())) return false;

    int cellsMoved = 0;

    while (cellsMoved < 2 &&
           pathIndex < static_cast<int>(path.size())) {
        row = path[pathIndex].first;
        col = path[pathIndex].second;
        pathIndex++;
        cellsMoved++;

        // Record every cell visited, including both cells in a fast move.
        moveHistory.push_back({row, col});
    }

    // One call to moveStep() represents one simulation step.
    if (cellsMoved > 0) {
        stepsTaken++;
        return true;
    }

    return false;
}