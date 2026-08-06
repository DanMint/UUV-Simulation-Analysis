#ifndef BASIC_SEEKER_AGENT_H
#define BASIC_SEEKER_AGENT_H

#include "seekerAgent.h"

struct BasicSeekerAgent : public SeekerAgent {
    BasicSeekerAgent(int id, int row, int col);
};

#endif // BASIC_SEEKER_AGENT_H