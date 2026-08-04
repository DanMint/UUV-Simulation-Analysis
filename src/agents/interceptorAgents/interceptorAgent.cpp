#include "interceptorAgent.h"
#include <cmath>

InterceptorAgent::InterceptorAgent(int id, int row, int col, double killRadius)
    : id(id), row(row), col(col),
      killRadius(killRadius),
      alive(true),
      killCount(0) {}

bool InterceptorAgent::isInRange(int checkRow, int checkCol) const {
    double dr = row - checkRow;
    double dc = col - checkCol;
    return std::sqrt(dr * dr + dc * dc) <= killRadius;
}

double InterceptorAgent::killProbability(int checkRow, int checkCol) const {
    double dr = row - checkRow;
    double dc = col - checkCol;
    double dist = std::sqrt(dr * dr + dc * dc);
    if (dist > killRadius) return 0.0;

    double ratio = (killRadius > 0.0) ? dist / killRadius : 0.0;
    if (ratio <= 0.5) return 0.90;  // inner 50% of radius
    if (ratio <= 0.7) return 0.60;  // 50-70% of radius
    return 0.50;                    // 70-100% of radius
}

void InterceptorAgent::recordIntercept(int seekerId, int step) {
    intercepts.push_back({seekerId, step});
    killCount++;
}