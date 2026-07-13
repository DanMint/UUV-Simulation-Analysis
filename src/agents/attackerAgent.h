#ifndef ATTACKER_AGENT_H
#define ATTACKER_AGENT_H

#include "seekerAgent.h"
#include <string>
#include <vector>

/**
 * AttackerAgent
 * Author: Nadeem
 *
 * Extends SeekerAgent with real-world UUV/UAV specs from the attacker
 * research database and a full Finite State Machine (FSM) lifecycle.
 *
 * ── FSM States ────────────────────────────────────────────────────────────
 *   S0  IDLE              Waiting for spawn command
 *   S1  RECEIVE_MISSION   Entry point and target accepted
 *   S2  VALIDATE          Input checked — water cell, distance, domain
 *   S3  INIT_BEHAVIOR     A* path computed, speed params configured
 *   S4  EXECUTE           Moving toward target each simulation step
 *   S5  LOG_RESULT        Milestones and outcome printed to terminal
 *   S6  UPDATE_SHARED     Result pushed to GA optimizer shared state
 *   S7  DEACTIVATE        Agent marked inactive, stays visible on map
 *   S8  COMPLETE          Terminal success state
 *   S9  RESET             Ready for next GA scenario
 *   FALLBACK              Error detected — reason logged
 *   ABORT                 Agent removed from simulation
 *
 * ── Supported agent types (use AttackerAgent::create()) ──────────────────
 *   UUVs:
 *     "bluerov2"    BlueROV2       ~$6k       1-3 kn   300k-450k Hz
 *     "riptide"     Riptide Micro  $15k-45k   2-5 kn   200k-400k Hz
 *     "blueboat"    BlueBoat       ~$5k       2-6 kn   450k-650k Hz (surface)
 *     "yuco"        Seaber YUCO    $50k-100k  2-6 kn   300k-600k Hz
 *     "nemosens"    RTSYS NemoSens $60k-115k  2-4 kn   200k-500k Hz
 *     "hugin"       HUGIN Superior $2M-4M     2-5 kn   200k-400k Hz
 *   UAVs (aerial — not detectable by hydrophone):
 *     "tb2"         Bayraktar TB2  $2M-5M     90-110 kn  N/A
 *     "queenhornet" Queen Hornet   $1k-5k     38-43 kn   N/A
 *     "shahed"      Shahed 136     $20k-50k   90-100 kn  N/A
 */

// ── FSM state enum ────────────────────────────────────────────────────────────

enum class AgentFSMState {
    S0_IDLE,
    S1_RECEIVE_MISSION,
    S2_VALIDATE,
    S3_INIT_BEHAVIOR,
    S4_EXECUTE,
    S5_LOG_RESULT,
    S6_UPDATE_SHARED,
    S7_DEACTIVATE,
    S8_COMPLETE,
    S9_RESET,
    FALLBACK,
    ABORT
};

// ── AttackerAgent ─────────────────────────────────────────────────────────────

struct AttackerAgent : public SeekerAgent {

    // ── Platform metadata ─────────────────────────────────────────────────────
    std::string agentType;        // e.g. "bluerov2", "hugin", "tb2"
    std::string manufacturer;     // e.g. "Blue Robotics", "Kongsberg"

    // ── Real-world specs ──────────────────────────────────────────────────────
    float speedKnotsMin;          // minimum speed in knots
    float speedKnotsMax;          // maximum speed in knots
    int   emissionFreqLowHz;      // lower bound of acoustic emission (Hz)
    int   emissionFreqHighHz;     // upper bound of acoustic emission (Hz)
    bool  shallowWaterCapable;    // can operate in shallow/harbor water?
    bool  isAerial;               // true = UAV, not detectable by hydrophone
    bool  isSurfaceVessel;        // true = USV (BlueBoat)
    float unitCostMin;            // estimated unit cost lower bound (USD)
    float unitCostMax;            // estimated unit cost upper bound (USD)

    // ── Detection / Engagement ─────────────────────────────────────────────────
    double sensingRadius;         // detection range for attacker sensors
    int    sightingCount;         // logged sighting count
    struct Sighting {
        int seekerId;
        int step;
    };
    std::vector<Sighting> sightings;

    double killRadius;            // engagement range for attacker intercepts
    int    killCount;             // successful intercepts logged
    struct Intercept {
        int seekerId;
        int step;
    };
    std::vector<Intercept> intercepts;

    // ── FSM state ─────────────────────────────────────────────────────────────
    AgentFSMState fsmState;       // current FSM state
    std::string   fallbackReason; // set when FSM enters FALLBACK

    // ── Movement ──────────────────────────────────────────────────────────────
    int stepDelay;                // steps between each grid cell move (speed sim)
    int stepDelayCounter;         // internal counter

    // ── Mission result ────────────────────────────────────────────────────────
    bool missionSuccess;          // true if agent reached target
    int  stepsToTarget;           // steps taken to reach target

    // ── Milestone flags (prevent duplicate prints) ────────────────────────────
    bool milestone25;
    bool milestone50;
    bool milestone75;

    // ── Constructor / Factory ─────────────────────────────────────────────────

    AttackerAgent(int id, int row, int col);

    /**
     * Create a fully configured AttackerAgent for the given type string.
     * Starts in S0_IDLE. Call tick() each simulation step to advance the FSM.
     */
    static AttackerAgent create(const std::string& type, int id, int row, int col);

    // ── FSM interface ─────────────────────────────────────────────────────────

    /**
     * Advance the FSM one step. Call once per simulation step.
     * S0-S3 and S5-S9 transition immediately on the same tick.
     * S4 stays active across ticks while the agent moves to target.
     * Returns true while the agent is still active (not S9/ABORT).
     */
    bool tick(int destRow, int destCol, const class Pathfinding& pf);
    bool tick(const class Pathfinding& pf);
    void setMissionTarget(int destRow, int destCol);

    /**
     * Standalone FSM demo — runs the full lifecycle and prints every state
     * transition to the terminal. Call before the main simulation to show
     * the FSM in action. Uses a dummy pathfinding run to completion.
     *
     * Example:
     *   AttackerAgent::runFSMDemo("bluerov2", pf, 5, 5, 50, 40);
     */
    static void runFSMDemo(const std::string& type,
                           const class Pathfinding& pf,
                           int spawnRow, int spawnCol,
                           int targetRow, int targetCol);

    /** Name of the current FSM state for logging. */
    std::string stateName() const;

    /** Force the agent into FALLBACK with a reason. Transitions to ABORT next tick. */
    void triggerFallback(const std::string& reason);

    // ── Helpers ───────────────────────────────────────────────────────────────

    /** Speed-aware move — skips steps based on stepDelay. */
    bool moveStepWithSpeed();

    /** True if an enemy position is within this attacker's sensing range. */
    bool isInRange(int checkRow, int checkCol) const;

    /** Log a sighting of a seeker at a given step. */
    void recordSighting(int seekerId, int step);

    /** Distance-tiered kill probability at the given position. */
    double killProbability(int checkRow, int checkCol) const;

    /** Log a successful intercept of a seeker at a given step. */
    void recordIntercept(int seekerId, int step);

    /** True if detectable by underwater hydrophone (not aerial, not surface). */
    bool isDetectableByHydrophone() const;

    /** True if emission frequency overlaps with given detector range. */
    bool isInFrequencyRange(int detectorLowHz, int detectorHighHz) const;

    /** One-line summary string for logging. */
    std::string summary() const;

private:
    int _destRow = -1;
    int _destCol = -1;

    void enterS1(int destRow, int destCol);
    void enterS2(int destRow, int destCol);
    void enterS3(int destRow, int destCol, const class Pathfinding& pf);
    bool runS4();
    void enterS5();
    void enterS6();
    void enterS7();
    void enterS8();
    void enterS9();
    void enterAbort();
};

#endif // ATTACKER_AGENT_H