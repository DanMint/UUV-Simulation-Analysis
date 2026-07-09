#include "patrolAgent.h"
#include <cmath>

// ── constructor ───────────────────────────────────────────────────────────────
PatrolDefenderAgent::PatrolDefenderAgent(int id, int row, int col,
                                          double sensingRadius, double killRadius,
                                          bool isDynamic, float speed)
    : id(id), row(row), col(col),
      alive(true),
      sensingRadius(sensingRadius),
      sightingCount(0),
      killRadius(killRadius),
      killCount(0),
      isDynamic(isDynamic),
      currentWaypoint(0),
      goingForward(true),
      speed(speed)
{}

// ── sensing ───────────────────────────────────────────────────────────────────
bool PatrolDefenderAgent::isInSensingRange(int checkRow, int checkCol) const {
    double dr = row - checkRow;
    double dc = col - checkCol;
    return std::sqrt(dr * dr + dc * dc) <= sensingRadius;
}

void PatrolDefenderAgent::recordSighting(int seekerId, int step) {
    sightings.push_back({seekerId, step});
    sightingCount++;
}

// ── killing ───────────────────────────────────────────────────────────────────
bool PatrolDefenderAgent::isInKillRange(int checkRow, int checkCol) const {
    double dr = row - checkRow;
    double dc = col - checkCol;
    return std::sqrt(dr * dr + dc * dc) <= killRadius;
}

double PatrolDefenderAgent::killProbability(int checkRow, int checkCol) const {
    double dr = row - checkRow;
    double dc = col - checkCol;
    double dist = std::sqrt(dr * dr + dc * dc);
    if (dist > killRadius) return 0.0;
    double ratio = dist / killRadius;
    // same tiered probability as InterceptorAgent
    if (ratio <= 0.5) return 0.9;   // inner 50% = 90% kill chance
    if (ratio <= 0.7) return 0.6;   // 50-70%    = 60% kill chance
    return 0.5;                      // 70-100%   = 50% kill chance
}

void PatrolDefenderAgent::recordIntercept(int seekerId, int step) {
    intercepts.push_back({seekerId, step});
    killCount++;
}

// ── movement ──────────────────────────────────────────────────────────────────
void PatrolDefenderAgent::addWaypoint(int r, int c) {
    waypoints.push_back({r, c});
}

void PatrolDefenderAgent::moveTowardWaypoint() {
    // need at least 2 waypoints to patrol
    if (waypoints.size() < 2) return;

    // get current target waypoint
    auto [targetRow, targetCol] = waypoints[currentWaypoint];

    // calculate direction
    double dr = targetRow - row;
    double dc = targetCol - col;
    double dist = std::sqrt(dr * dr + dc * dc);

    if (dist <= speed) {
        // reached this waypoint — snap to it and advance
        row = targetRow;
        col = targetCol;

        // move to next waypoint, bounce at ends
        if (goingForward) {
            currentWaypoint++;
            if (currentWaypoint >= (int)waypoints.size()) {
                currentWaypoint = (int)waypoints.size() - 2;
                goingForward = false;
            }
        } else {
            currentWaypoint--;
            if (currentWaypoint < 0) {
                currentWaypoint = 1;
                goingForward = true;
            }
        }
    } else {
        // move one step toward waypoint
        row += (int)std::round((dr / dist) * speed);
        col += (int)std::round((dc / dist) * speed);
    }
}