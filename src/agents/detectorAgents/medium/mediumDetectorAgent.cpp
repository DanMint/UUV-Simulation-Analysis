#include "mediumDetectorAgent.h"

MediumDetectorAgent::MediumDetectorAgent(
    int id,
    int row,
    int col,
    double sensingRadius
)
    : DetectorAgent(
          id,
          row,
          col,
          sensingRadius * 1.25
      )
{
    this->cost = 2;
}

double MediumDetectorAgent::radarEvasionResistance() const {
    return 0.50;
}