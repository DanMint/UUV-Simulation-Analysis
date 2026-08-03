#ifndef TARGET_AGENT_H
#define TARGET_AGENT_H

/**
 * TargetAgent
 *
 * Stationary defender. Sits at a position and waits to be destroyed.
 * Destroyed when a seeker reaches its cell (collision).
 *
 * isCritical: designates this target as a "critical harbour asset". Lance's
 * framing is about the probability that attack agents reach a critical
 * harbour asset — so we need a distinguishable, designated asset, not just
 * "whichever target got created first". The cost-benefit CSV tracks whether
 * this specific critical asset was reached.
 */
struct TargetAgent {
    int id;
    int row;
    int col;
    bool alive;
    bool isCritical;

    TargetAgent(int id, int row, int col, bool isCritical = false);
};

#endif // TARGET_AGENT_H