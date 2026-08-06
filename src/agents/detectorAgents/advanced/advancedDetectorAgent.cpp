#include "advancedDetectorAgent.h"

AdvancedDetectorAgent::AdvancedDetectorAgent(
    int id,
    int row,
    int col,
    double sensingRadius
)
    : DetectorAgent(
          id,
          row,
          col,
          sensingRadius * 1.50
      )
{
    this->cost = 3;
}

double AdvancedDetectorAgent::radarEvasionResistance() const {
    return 0.80;
}