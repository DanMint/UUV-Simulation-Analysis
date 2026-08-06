#ifndef EVADER_SEEKER_AGENT_H
#define EVADER_SEEKER_AGENT_H

#include "seekerAgent.h"

struct EvaderSeekerAgent : public SeekerAgent {
    EvaderSeekerAgent(int id, int row, int col);

    /** 50% chance to evade each detector observation attempt. */
    double radarEvasionProbability() const override;

    /** 50% chance to evade each interceptor engagement attempt. */
    double interceptorEvasionProbability() const override;
};

#endif // EVADER_SEEKER_AGENT_H