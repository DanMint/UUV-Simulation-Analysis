#include "basicSeekerAgent.h"

BasicSeekerAgent::BasicSeekerAgent(int id, int row, int col)
    : SeekerAgent(id, row, col)
{
    this->cost = 1;
}