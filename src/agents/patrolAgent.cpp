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
    return std::sqrt(dr * dr + dc * dc) <= sensingRadius;   //Use distance formula 1 formula 3 lines --- also think if we conmpute a distacne of 7 <= radius of 10 welp 7 is less than 10 so true therefore enemy detected TRUE--- I know its in the orignal code but for my breakdown and understnding 
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
    //My understanding: are we outside kill radius? > Yes > 0% ....Otherwise compute distacne and the percentage 
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
    moveHistory.push_back({row, col});

    // need at least 2 waypoints to patrol --- if i dont have the 2 waypoints dont move
    if (waypoints.size() < 2) return;

    // get current target waypoint - had initally used auto but changed to using .first and .second to get the row/col values of the pair so think waypoints.push_back({r, c});  // r goes into .first, c goes into .second since wer using a pair of ints to store the row/col values of the waypoint
    //auto [targetRow, targetCol] = waypoints[currentWaypoint];

    int targetRow = waypoints[currentWaypoint].first;
    int targetCol = waypoints[currentWaypoint].second;

    // calculate direction
    double dr = targetRow - row;
    double dc = targetCol - col;
    double dist = std::sqrt(dr * dr + dc * dc);

    if (dist <= speed) {
        // reached this waypoint — snap to it and advance --- ie stop and turn around but whats forward/backwards thats the following if/else --- think if im dist of 12 and speed of 3 i can still move but if im 0 dist and speed of 2 then im on it ie stop 
        row = targetRow;
        col = targetCol;

        // move to next waypoint, bounce at ends
        if (goingForward) {
        currentWaypoint++; //Use this to dictate which direction were going forward/reverse --- and ++/-- otherwise out of bounds ie look at the math waypoints.size-2 = -1 as waypoints.size() is the number of elements in an array/vector cause cpp which is 2 in total but its 0,1 
            if (currentWaypoint >= (int)waypoints.size()) {
                currentWaypoint = (int)waypoints.size() - 2;
                goingForward = false; // so when this chunk is done we would then revers which is the else flipping beween the two here 
            }
        } else {
            currentWaypoint--;
            if (currentWaypoint < 0) {
                currentWaypoint = 1;
                goingForward = true;
            }
        }
    } 
    else {
        // move one step toward waypoint --- this is the actual movement --- heres what happens were taking one step per tick and in the simlation.cpp we have a moveTowardWaypoint(); which calls this fucnton every tick 
        row += (int)std::round((dr / dist) * speed);//dr or really d anything is delta row...
        col += (int)std::round((dc / dist) * speed);
    }
}

