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

    // ── identity --- Needed to identfy which PatrolDefende (id), where (row/col), if active )alive
    int id; 
    int row, col;
    bool alive;

    // ── from DetectorAgent — sensing ──────────────────────────────
    double sensingRadius; // how far out can i detect 
    int sightingCount; // just a number count of how many times spotted
    struct Sighting { int seekerId; int step; }; //struct bundling which seeker was spotted, when it was spotted 
    std::vector<Sighting> sightings; // have a list that stores the spots as an entry with the info above

    // ── from InterceptorAgent — killing, code is essentlly same as above but for if your dead 
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

    // ── constructor --- Think function headers/decleration
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