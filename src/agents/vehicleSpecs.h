#ifndef VEHICLE_SPECS_H
#define VEHICLE_SPECS_H

#include <string>

/**
 * VehicleSpecs
 *
 * Shared data structure for all UUV/UAV vehicle types.
 * This is the single source of truth for vehicle parameters:
 * speed, cost, acoustic emission frequency, capabilities, etc.
 *
 * Usage:
 *   VehicleSpecs specs = getVehicleSpecs("bluerov2");
 *   AttackerAgent attacker = AttackerAgent(id, row, col);
 *   attacker.applySpecs(specs);
 *
 * Supported types:
 *   - bluerov2, riptide, blueboat, yuco, nemosens, hugin (UUVs)
 *   - tb2, queenhornet, shahed (UAVs)
 */

struct VehicleSpecs {
    std::string agentType;            // e.g., "bluerov2", "tb2"
    std::string manufacturer;         // e.g., "Blue Robotics", "Bayraktar"
    
    float speedKnotsMin;              // min speed (knots)
    float speedKnotsMax;              // max speed (knots)
    
    int emissionFreqLowHz;            // acoustic emission lower (Hz)
    int emissionFreqHighHz;           // acoustic emission upper (Hz)
    
    bool shallowWaterCapable;         // true = can operate in shallow/harbor
    bool isAerial;                    // true = UAV, not detectable by sonar
    bool isSurfaceVessel;             // true = USV (surface)
    
    float unitCostMin;                // unit cost lower (USD)
    float unitCostMax;                // unit cost upper (USD)
    
    int stepDelay;                    // steps between each grid move (speed simulation)
};

/**
 * Get vehicle specs for a given type string.
 * Returns a fully configured VehicleSpecs struct.
 * Throws std::invalid_argument if type is unknown.
 */
VehicleSpecs getVehicleSpecs(const std::string& type);

#endif // VEHICLE_SPECS_H
