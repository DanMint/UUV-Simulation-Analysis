#include "advancedInterceptorAgent.h"

#include <cmath>

AdvancedInterceptorAgent::AdvancedInterceptorAgent(
    int id,
    int row,
    int col,
    double killRadius
)
    : InterceptorAgent(id, row, col, killRadius)
{
    this->cost = 3;
}

double AdvancedInterceptorAgent::killProbability(
    int checkRow,
    int checkCol
) const {
    const double rowDifference = row - checkRow;
    const double columnDifference = col - checkCol;

    const double distance = std::sqrt(
        rowDifference * rowDifference +
        columnDifference * columnDifference
    );

    if (distance > killRadius) {
        return 0.0;
    }

    const double ratio = killRadius > 0.0
        ? distance / killRadius
        : 0.0;

    if (ratio <= 0.5) {
        return 0.99;
    }

    if (ratio <= 0.7) {
        return 0.90;
    }

    return 0.80;
}