#ifndef TARGET_AGENT_H
#define TARGET_AGENT_H

/**
 * TargetAgent
 *
 * Stationary defender. Sits at a position and waits to be destroyed.
 * Destroyed when a seeker reaches its cell (collision).
 */
struct TargetAgent {
    int id;
    int row;
    int col;
    bool alive;

    TargetAgent(int id, int row, int col);
};

#endif // TARGET_AGENT_H