#include "detectorAgent.h"

#include <cmath>

DetectorAgent::DetectorAgent(
    int id,
    int row,
    int col,
    double sensingRadius
)
    : id(id),
      row(row),
      col(col),
      sensingRadius(sensingRadius),
      alive(true),
      sightingCount(0),
      cost(0)
{
}

bool DetectorAgent::isInRange(
    int checkRow,
    int checkCol
) const {
    const double rowDifference = row - checkRow;
    const double columnDifference = col - checkCol;

    return std::sqrt(
        rowDifference * rowDifference +
        columnDifference * columnDifference
    ) <= sensingRadius;
}

void DetectorAgent::recordSighting(
    int seekerId,
    int step
) {
    sightings.push_back({seekerId, step});
    ++sightingCount;
}

double DetectorAgent::radarEvasionResistance() const {
    return 0.0;
}