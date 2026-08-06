#include "evaderSeekerAgent.h"

EvaderSeekerAgent::EvaderSeekerAgent(int id, int row, int col)
    : SeekerAgent(id, row, col)
{
    this->cost = 3;
}

double EvaderSeekerAgent::radarEvasionProbability() const {
    return 0.5;
}

double EvaderSeekerAgent::interceptorEvasionProbability() const {
    return 0.5;
}