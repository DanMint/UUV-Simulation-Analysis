#ifndef BASIC_SEEKER_AGENT_H
#define BASIC_SEEKER_AGENT_H

#include "../seekerAgent.h"

/**
 * BasicSeekerAgent
 *
 * Uses SeekerAgent's normal A* movement and sets cost to 1.
 */
struct BasicSeekerAgent : public SeekerAgent {
    BasicSeekerAgent(int id, int row, int col);
};

#endif // BASIC_SEEKER_AGENT_H