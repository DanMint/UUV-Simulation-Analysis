#ifndef PATROL_AGENT_H
#define PATROL_AGENT_H

#include <vector>
#include <cmath>

/*
    PatrolDefenderAgent
    
    Combines the sensing of DetectorAgent and the killing of InterceptorAgent
    into one mobile agent. The only new thing is waypoint movement.
    
    isDynamic == false → sits still like a normal detector/interceptor
    isDynamic == true  → moves between waypoints before sensing/killing each tick
*/

struct PatrolDefenderAgent {

    // ── identity ──────────────────────────────────────────────────
    int id;
    int row, col;
    bool alive;

    // ── from DetectorAgent — sensing ──────────────────────────────
    double sensingRadius;
    int sightingCount;
    struct Sighting { int seekerId; int step; };
    std::vector<Sighting> sightings;

    // ── from InterceptorAgent — killing ───────────────────────────
    double killRadius;
    int killCount;
    struct Intercept { int seekerId; int step; };
    std::vector<Intercept> intercepts;

    // ── NEW — patrol movement ─────────────────────────────────────
    bool isDynamic;
    std::vector<std::pair<int,int>> waypoints;  // list of (row, col) points
    int currentWaypoint;                         // which waypoint heading toward
    bool goingForward;                           // A→B or B→A
    float speed;                                 // cells per tick

    // ── constructor ───────────────────────────────────────────────
    PatrolDefenderAgent(int id, int row, int col,
                        double sensingRadius, double killRadius,
                        bool isDynamic = false, float speed = 1.0f);

    // ── sensing — same as DetectorAgent ───────────────────────────
    bool isInSensingRange(int checkRow, int checkCol) const;
    void recordSighting(int seekerId, int step);

    // ── killing — same as InterceptorAgent ────────────────────────
    bool isInKillRange(int checkRow, int checkCol) const;
    double killProbability(int checkRow, int checkCol) const;
    void recordIntercept(int seekerId, int step);

    // ── movement — NEW ────────────────────────────────────────────
    void addWaypoint(int row, int col);
    void moveTowardWaypoint();
};

#endif // PATROL_AGENT_H