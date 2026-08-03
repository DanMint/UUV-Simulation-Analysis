#include "interceptorAgent.h"
#include "vehicleSpecs.h"
#include <cmath>

InterceptorAgent::InterceptorAgent(int id, int row, int col, double killRadius)
    : id(id), row(row), col(col),
      killRadius(killRadius),
      alive(true),
      killCount(0),
      engagementCount(0),
      engagementCost(DEFAULT_COST_PER_SHOT),
      vehicleType("") {}

InterceptorAgent::InterceptorAgent(int id, int row, int col, double killRadius,
                                   const std::string& vehicleType)
    : id(id), row(row), col(col),
      killRadius(killRadius),
      alive(true),
      killCount(0),
      engagementCount(0),
      engagementCost(costPerShotForType(vehicleType)),
      vehicleType(vehicleType) {}

float InterceptorAgent::costPerShotForType(const std::string& vehicleType) {
    if (vehicleType.empty()) return DEFAULT_COST_PER_SHOT;
    try {
        const VehicleSpecs specs = getVehicleSpecs(vehicleType);
        // Realistic munition-economic model: a premium platform's effective
        // engagement cost scales with its acquisition cost. Approximate the
        // cost of a single engagement shot as a fraction of the platform's
        // SWAP (Size, Weight, Power) tier:
        //   budget  (<$10k)   → ~$10k   per shot
        //   mid     ($10k-100k)→ ~$50k  per shot
        //   premium ($100k-1M)→ ~$250k per shot
        //   flagship(>$1M)    → ~$1M   per shot
        const float avgCost = (specs.unitCostMin + specs.unitCostMax) / 2.0f;
        if (avgCost < 10000.0f)      return 10000.0f;   // budget
        if (avgCost < 100000.0f)     return 50000.0f;   // mid-range
        if (avgCost < 1000000.0f)    return 250000.0f;  // premium
        return 1000000.0f;                               // flagship
    } catch (...) {
        return DEFAULT_COST_PER_SHOT;
    }
}

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