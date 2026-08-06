#include "interceptorAgent.h"

#include <cmath>

InterceptorAgent::InterceptorAgent(
    int id,
    int row,
    int col,
    double killRadius
)
    : id(id),
      row(row),
      col(col),
      killRadius(killRadius),
      alive(true),
      cost(0),
      killCount(0)
{
}

bool InterceptorAgent::isInRange(
    int checkRow,
    int checkCol
) const {
    const double rowDifference = row - checkRow;
    const double columnDifference = col - checkCol;

    const double distance = std::sqrt(
        rowDifference * rowDifference +
        columnDifference * columnDifference
    );

    return distance <= killRadius;
}

double InterceptorAgent::killProbability(
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
        return 0.90;
    }

    if (ratio <= 0.7) {
        return 0.60;
    }

    return 0.50;
}

void InterceptorAgent::recordIntercept(
    int seekerId,
    int step
) {
    intercepts.push_back({seekerId, step});
    ++killCount;
}