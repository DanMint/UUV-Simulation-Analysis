#include "basicDetectorAgent.h"

BasicDetectorAgent::BasicDetectorAgent(
    int id,
    int row,
    int col,
    double sensingRadius
)
    : DetectorAgent(id, row, col, sensingRadius)
{
    this->cost = 1;
}