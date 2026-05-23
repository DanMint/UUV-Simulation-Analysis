#include "detectorAgent.h"
#include <cmath>

DetectorAgent::DetectorAgent(int id, int row, int col, double sensingRadius)
    : id(id), row(row), col(col),
      sensingRadius(sensingRadius),
      alive(true),
      sightingCount(0) {}

bool DetectorAgent::isInRange(int checkRow, int checkCol) const {
    double dr = row - checkRow;
    double dc = col - checkCol;
    return std::sqrt(dr * dr + dc * dc) <= sensingRadius;
}

void DetectorAgent::recordSighting(int seekerId, int step) {
    sightings.push_back({seekerId, step});
    sightingCount++;
}