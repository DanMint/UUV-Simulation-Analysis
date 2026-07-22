#ifndef ATTACKER_AGENT_H
#define ATTACKER_AGENT_H

#include "seekerAgent.h"
#include "vehicleSpecs.h"
#include <string>
#include <vector>
#include <optional>
#include <string_view>

// ═══════════════════════════════════════════════════════════════════════════════
//  AttackerAgent — Real-World UUV/UAV Simulation with Full FSM Lifecycle
// ═══════════════════════════════════════════════════════════════════════════════
//
//  Design Philosophy:
//  ────────────────
//  AttackerAgent extends SeekerAgent with real-world vehicle specifications
//  (speed, cost, acoustic emission frequency) and a comprehensive 12-state
//  Finite State Machine (FSM) lifecycle.
//
//  Agent Categories (use AttackerAgent::create()):
//  ──────────────
//  UUVs (Underwater):
//    "bluerov2"    BlueROV2       ~$6k      1-3 kn   300k-450k Hz  |  Slow, cheap
//    "riptide"     Riptide Micro  $15k-45k  2-5 kn   200k-400k Hz  |  Medium, versatile
//    "blueboat"    BlueBoat       ~$5k      2-6 kn   450k-650k Hz  |  Surface vessel
//    "yuco"        Seaber YUCO    $50k-100k 2-6 kn   300k-600k Hz  |  Fast, mid-cost
//    "nemosens"    RTSYS NemoSens $60k-115k 2-4 kn   200k-500k Hz  |  Sensor platform
//    "hugin"       HUGIN Superior $2M-4M    2-5 kn   200k-400k Hz  |  Premium UUV
//
//  UAVs (Aerial — NOT hydrophone-detectable):
//    "tb2"         Bayraktar TB2  $2M-5M    90-110 kn  N/A         |  Combat drone
//    "queenhornet" Queen Hornet   $1k-5k    38-43 kn   N/A         |  Swarm drone
//    "shahed"      Shahed 136     $20k-50k  90-100 kn  N/A         |  Loitering munition
//
//  FSM States:
//  ──────────
//    S0  IDLE              → Waiting for spawn command
//    S1  RECEIVE_MISSION   → Entry point — target accepted
//    S2  VALIDATE          → Input validation (water cell, distance, domain)
//    S3  INIT_BEHAVIOR     → A* path computed, speed params configured
//    S4  EXECUTE           → Moving toward target each simulation step
//    S5  LOG_RESULT        → Milestones and outcome logged
//    S6  UPDATE_SHARED     → Result pushed to GA optimizer shared state
//    S7  DEACTIVATE        → Agent marked inactive (stays visible on map)
//    S8  COMPLETE          → Terminal success state
//    S9  RESET             → Ready for next GA scenario
//    FALLBACK              → Error detected — reason logged
//    ABORT                 → Agent removed from simulation
//
//  Author: Nadeem
// ═══════════════════════════════════════════════════════════════════════════════

// ── FSM State Enum ─────────────────────────────────────────────────────────────
// Represents the full lifecycle of an attacker agent from spawn to completion.
// States S0-S9 represent normal operation; FALLBACK and ABORT represent errors.

enum class AgentFSMState : uint8_t {
    S0_IDLE,              ///< Awaiting spawn/mission assignment
    S1_RECEIVE_MISSION,   ///< Mission target received
    S2_VALIDATE,          ///< Validating mission parameters
    S3_INIT_BEHAVIOR,     ///< Computing path, configuring speed
    S4_EXECUTE,           ///< Moving along path toward target
    S5_LOG_RESULT,        ///< Logging mission outcome
    S6_UPDATE_SHARED,     ///< Updating GA optimizer shared state
    S7_DEACTIVATE,        ///< Agent deactivated (visible on map)
    S8_COMPLETE,          ///< Terminal success state
    S9_RESET,             ///< Reset for next scenario
    FALLBACK,             ///< Error detected
    ABORT                 ///< Agent removed from simulation
};

// ── AttackerAgent ──────────────────────────────────────────────────────────────

struct AttackerAgent : public SeekerAgent {

    // ═══════════════════════════════════════════════════════════════════════════
    //  Fields
    // ═══════════════════════════════════════════════════════════════════════════

    // ── Vehicle Specs ──────────────────────────────────────────────────────────
    VehicleSpecs specs;                     ///< Real-world vehicle parameters

    // ── Detection / Sensing ────────────────────────────────────────────────────
    double sensingRadius        = 5.0;      ///< Detection range for attacker sensors (cells)
    int    sightingCount        = 0;        ///< Total logged sightings
    struct Sighting {
        int seekerId;                       ///< ID of detected seeker
        int step;                           ///< Simulation step of detection
    };
    std::vector<Sighting> sightings;        ///< All logged sightings

    // ── Engagement / Interception ──────────────────────────────────────────────
    double killRadius           = 3.0;      ///< Engagement range for intercepts (cells)
    int    killCount            = 0;        ///< Total successful intercepts
    struct Intercept {
        int seekerId;                       ///< ID of intercepted seeker
        int step;                           ///< Simulation step of intercept
    };
    std::vector<Intercept> intercepts;      ///< All logged intercepts

    // ── FSM State ──────────────────────────────────────────────────────────────
    AgentFSMState fsmState      = AgentFSMState::S0_IDLE;  ///< Current FSM state
    std::string   fallbackReason;           ///< Reason for FALLBACK state

    // ── Movement ───────────────────────────────────────────────────────────────
    int stepDelayCounter        = 0;        ///< Internal counter for speed-aware movement

    // ── Mission Results ────────────────────────────────────────────────────────
    bool missionSuccess         = false;    ///< Whether agent reached target
    int  stepsToTarget          = 0;        ///< Steps taken to reach target
    bool everSucceeded          = false;    ///< Sticky — never cleared by enterS9()
    int  bestStepsToTarget      = 0;        ///< Steps in the best successful run

    // ── Milestone Flags (prevent duplicate prints) ─────────────────────────────
    bool milestone25            = false;    ///< 25% progress milestone reached
    bool milestone50            = false;    ///< 50% progress milestone reached
    bool milestone75            = false;    ///< 75% progress milestone reached

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constants
    // ═══════════════════════════════════════════════════════════════════════════
    static constexpr int kMaxStepsBeforeAbort = 2000;   ///< Max steps before forced abort
    static constexpr double kDefaultSensingRadius = 5.0; ///< Default detection range
    static constexpr double kDefaultKillRadius    = 3.0; ///< Default engagement range

    // ═══════════════════════════════════════════════════════════════════════════
    //  Constructor / Factory
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Construct an AttackerAgent with default (BlueROV2) specs.
     * @param id  Unique agent identifier
     * @param row Spawn row on the grid
     * @param col Spawn column on the grid
     *
     * Use create() for vehicle-specific configuration.
     */
    AttackerAgent(int id, int row, int col);

    /**
     * @brief Create a fully configured AttackerAgent for the given type string.
     * @param type  Vehicle type (e.g., "bluerov2", "tb2", "hugin")
     * @param id    Unique agent identifier
     * @param row   Spawn row on the grid
     * @param col   Spawn column on the grid
     * @return Fully initialized AttackerAgent in S0_IDLE state
     *
     * @throws std::invalid_argument If type is unknown (caught internally,
     *         falls back to "bluerov2" with a warning).
     *
     * Usage:
     *   auto agent = AttackerAgent::create("hugin", 0, 15, 30);
     *   agent.setMissionTarget(targetRow, targetCol);
     *   while (agent.tick(pf)) { /* advance FSM *\/ }
     */
    [[nodiscard]] static AttackerAgent create(
        const std::string& type, int id, int row, int col);

    // ═══════════════════════════════════════════════════════════════════════════
    //  FSM Interface
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Advance the FSM by one simulation step.
     *
     * State progression per tick:
     *   - S0_IDLE → S1_RECEIVE_MISSION → S2_VALIDATE (collapsed, first tick)
     *   - S2_VALIDATE → S3_INIT_BEHAVIOR (path computation, first tick)
     *   - S3_INIT_BEHAVIOR → S4_EXECUTE (second tick)
     *   - S4_EXECUTE stays active across multiple ticks while moving
     *   - On reaching target: S5 → S6 → S7 → S8 → S9 (one tick each)
     *   - On error: FALLBACK → ABORT (one tick each)
     *
     * @param destRow Target row on the grid
     * @param destCol Target column on the grid
     * @param pf      Pathfinding engine reference
     * @return true   Agent is still active (not in S9_RESET or ABORT)
     * @return false  Agent has reached a terminal state
     */
    [[nodiscard]] bool tick(int destRow, int destCol, const class Pathfinding& pf);

    /**
     * @brief Advance the FSM using previously-set mission target.
     * @param pf Pathfinding engine reference
     * @return true if agent is still active
     *
     * Requires setMissionTarget() to have been called first.
     */
    [[nodiscard]] bool tick(const class Pathfinding& pf);

    /**
     * @brief Set the mission target for this agent.
     * @param destRow Target row
     * @param destCol Target column
     *
     * Must be called before tick(pf) (the no-target overload).
     */
    void setMissionTarget(int destRow, int destCol);

    /**
     * @brief Run a standalone FSM demo for any vehicle type.
     *
     * Demonstrates the full FSM lifecycle from S0 through S9 with
     * all state transitions printed to the terminal.
     *
     * @param type       Vehicle type string
     * @param pf         Pathfinding engine
     * @param spawnRow   Spawn row
     * @param spawnCol   Spawn column
     * @param targetRow  Target row
     * @param targetCol  Target column
     *
     * Example:
     *   Pathfinding pf(grid);
     *   AttackerAgent::runFSMDemo("bluerov2", pf, 5, 5, 50, 40);
     */
    static void runFSMDemo(const std::string& type,
                           const class Pathfinding& pf,
                           int spawnRow, int spawnCol,
                           int targetRow, int targetCol);

    // ═══════════════════════════════════════════════════════════════════════════
    //  State Queries
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Get the human-readable name of the current FSM state.
     * @return std::string like "S4_EXECUTE" or "FALLBACK"
     */
    [[nodiscard]] std::string stateName() const;

    /**
     * @brief Force the agent into FALLBACK with a descriptive reason.
     * @param reason  Human-readable explanation of the failure
     *
     * Sets alive=false and transitions to FALLBACK. Next tick moves to ABORT.
     */
    void triggerFallback(const std::string& reason);

    // ═══════════════════════════════════════════════════════════════════════════
    //  Movement Helpers
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Speed-aware movement step.
     *
     * Uses stepDelay to throttle movement: an agent with stepDelay=4
     * moves once every 4 ticks, simulating slower vehicles.
     *
     * @return true if the agent actually moved this tick
     */
    [[nodiscard]] bool moveStepWithSpeed();

    // ═══════════════════════════════════════════════════════════════════════════
    //  Detection Helpers
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Check if a position is within this attacker's sensing radius.
     * @param checkRow  Row to check
     * @param checkCol  Column to check
     * @return true if Euclidean distance <= sensingRadius
     */
    [[nodiscard]] bool isInRange(int checkRow, int checkCol) const;

    /**
     * @brief Log a sighting of a seeker at a given step.
     * @param seekerId  ID of the detected seeker
     * @param step      Current simulation step
     */
    void recordSighting(int seekerId, int step);

    /**
     * @brief Calculate distance-tiered kill probability at a given position.
     *
     * Probability tiers:
     *   Inner 50% of radius (ratio <= 0.5) → 90%
     *   50-70% of radius   (ratio <= 0.7) → 60%
     *   70-100% of radius                  → 50%
     *
     * @param checkRow  Row of the target
     * @param checkCol  Column of the target
     * @return double   Kill probability [0.0, 1.0]; 0.0 if out of range
     */
    [[nodiscard]] double killProbability(int checkRow, int checkCol) const;

    /**
     * @brief Log a successful intercept of a seeker at a given step.
     * @param seekerId  ID of the intercepted seeker
     * @param step      Current simulation step
     */
    void recordIntercept(int seekerId, int step);

    /**
     * @brief Check if this agent is detectable by an underwater hydrophone.
     * @return false for aerial agents and surface vessels; true for underwater UUVs
     */
    [[nodiscard]] bool isDetectableByHydrophone() const;

    /**
     * @brief Check if this agent's emission frequency overlaps a detector's range.
     * @param detectorLowHz   Lower bound of detector frequency range (Hz)
     * @param detectorHighHz  Upper bound of detector frequency range (Hz)
     * @return true if frequency bands overlap AND agent is hydrophone-detectable
     *
     * Always returns false for aerial agents (isDetectableByHydrophone() gates this).
     */
    [[nodiscard]] bool isInFrequencyRange(int detectorLowHz, int detectorHighHz) const;

    /**
     * @brief Get a one-line summary string for logging.
     * @return Formatted string: "[type id=N] state= pos=(r,c) alive= detected= steps= cost=$"
     */
    [[nodiscard]] std::string summary() const;

    // ═══════════════════════════════════════════════════════════════════════════
    //  Utility
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Validate that the agent is in a consistent state.
     * @return true if no inconsistencies detected
     *
     * Checks:
     *   - FSM state is valid enum value
     *   - alive flag consistent with FSM state
     *   - Position is within grid bounds
     */
    [[nodiscard]] bool validateState() const;

private:
    // ═══════════════════════════════════════════════════════════════════════════
    //  Private Data
    // ═══════════════════════════════════════════════════════════════════════════
    int _destRow = -1;      ///< Current mission target row
    int _destCol = -1;      ///< Current mission target column

    // ═══════════════════════════════════════════════════════════════════════════
    //  FSM State Transitions (private — called by tick())
    // ═══════════════════════════════════════════════════════════════════════════
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

