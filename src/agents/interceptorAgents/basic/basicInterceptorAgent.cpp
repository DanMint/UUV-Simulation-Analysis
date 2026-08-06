#include "basicInterceptorAgent.h"

BasicInterceptorAgent::BasicInterceptorAgent(
    int id,
    int row,
    int col,
    double killRadius
)
    : InterceptorAgent(id, row, col, killRadius)
{
    this->cost = 1;
}