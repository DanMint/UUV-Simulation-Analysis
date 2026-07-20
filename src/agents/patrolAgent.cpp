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
    // need at least 2 waypoints to patrol --- if i dont have the 2 waypoints dont move
    if (waypoints.size() < 2) return;

    // get current target waypoint
    auto [targetRow, targetCol] = waypoints[currentWaypoint];

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

/*
I will be going thru all my code stuff in this order and commenting to both better my understanding of the current code and new code as my agents share code block thats already here going in this order for now, btw all my code changes will be refered to as Chris added ___ so when i look back i can just ctrl + f :

    -patrolAgent.h — defined what your agent IS (its variables)
    -patrolAgent.cpp — defined what your agent DOES (its functions)
    -agent.h — just told the project "hey this new agent exists"
    -simulation.h — added a list to store patrol defenders in memory
    -simulation.cpp — plugged it into the loop so it actually runs each tick
    -mapVisualizer.cpp — added P key so you can place it on screen adn coloring
    -CMakeList.txt - need to make sure its being read properly via correct pathing/folder
    -SimResults - this is beacuese it translates out build results to this file whcih will then turn it into a json then the visualize.py reads the json heres the break down :sim runs > buildResult packages it > saveJSON writes the file > visualize.py reads it > you see the replay
    -do i need to look into main or really any iother fuile 
    -mapcreation.h - needed to add cell type 6 

    To do the A adn B and set number for each patrol group think map visual and need the actula logi of heres A and B and actully know casue apparently it doenst know i set all the foundation and did most of it ahhhhhhhhhhhhhhhh

    ctrl+k then S to save all the file changes 

    to build when done use this not just build then uuv 
    ./build/uuv_sim maps/pearlHarbour/harbour_Depth_Area.shp

    new issue for later has it all but doesnt show wher the first A and B is on the map just the coords but the patrol peiece is in the middle and then figure out hwo to have it say patrol when pressing p in the bottom left

    -currently its all nice it shows the coords in the terminal still kinda iffy on how to display it fully casue stil a bunch of diamonds
    -deletion of a patrol just get rid of A 
    -Code wise it "inherits" but its really copy paste code of detection of code why not have that ghost circle in the map when placing various simulation tools idk what to call them "yo bout to drop some seekers????"

    Load a Json:
    ./build/uuv_sim --scenario scenario.json

    CURRENT SAVING OF JSON AND WHATNOT IDK MAN:
    -need to add the dlivmiter of , before "patrol_defender": othersiwde code freask out its line 277 in all jsons 
    - IS THIS PROJECT A VIDEO OP PICTURE FOR VISUALIZER 
    
*/