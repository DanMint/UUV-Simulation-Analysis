#ifndef FAST_SEEKER_AGENT_H
#define FAST_SEEKER_AGENT_H

#include "seekerAgent.h"

struct FastSeekerAgent : public SeekerAgent {
    FastSeekerAgent(int id, int row, int col);

    /** Move up to two path cells during one simulation step. */
    bool moveStep() override;
};

#endif // FAST_SEEKER_AGENT_H