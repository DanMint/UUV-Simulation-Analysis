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




    For actully running the program:
    1. use      cd build
    2. use      cmake ..
    3. use      make 
    4. use      cd ..
    5. use      ./build/uuv_sim maps/pearlHarbour/harbour_Depth_Area.shp


    From doing map to getting vidsual:
    1. use      rm -f paths/*.png       to clear all prior pngs also      rm -rf runs/*     Clear runs
    2. use      ./build/uuv_sim --scenario scenario.json        to build the last scenario you will need to re input the values ig
    3. use      python3 visulaize.py    to actully take the jsons and turn into the pngs 


    What this code agent features:
        -The main goal was to be able to place 2 points (A and B) of which would go back and forth between the two whilst aslo acting as a mobile detector at the same time 
    

    Neat Features and parts of the code include:
        - being able to see a path from out two placed points via a --- doted line 
        - The location for them is also there but its just in the terminal ie (A is place at 43, 38) 
        - Also feature a "safety net" in the form of "waiting for placement point B" or "You placed B on on land try again"
        - the deltion process of a path/agent in the map phase of code just right click the first point (A)
    

    Semi Imporatnt things to make this project easier:
        - use       Ctrl + K then S to save all files rather than one at a time
        - as per my notes of what each section does above and how to sorta know where to look use Ctrl + F then "Chris added: " as  I had done this for all my parts of code since there is so much in all these current .cpp and .h files

    

    

    - IS THIS PROJECT A VIDEO OP PICTURE FOR VISUALIZER its odd its liek it runs then it takes pictures at points and thats where we get the json images in the form of pngs 




*/