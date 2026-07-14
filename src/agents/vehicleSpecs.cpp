#include "vehicleSpecs.h"
#include <stdexcept>

VehicleSpecs getVehicleSpecs(const std::string& type) {
    
    // ════════════════════════════════════════════════════════════════════════════════
    //  UUVs (Underwater Unmanned Vehicles)
    // ════════════════════════════════════════════════════════════════════════════════
    
    if (type == "bluerov2") {
        return {
            .agentType         = "bluerov2",
            .manufacturer      = "Blue Robotics",
            .speedKnotsMin     = 1.0f,
            .speedKnotsMax     = 3.0f,
            .emissionFreqLowHz = 300000,
            .emissionFreqHighHz = 450000,
            .shallowWaterCapable = true,
            .isAerial          = false,
            .isSurfaceVessel   = false,
            .unitCostMin       = 6000.0f,
            .unitCostMax       = 6000.0f,
            .stepDelay         = 4
        };
    }
    
    if (type == "riptide") {
        return {
            .agentType         = "riptide",
            .manufacturer      = "Oceanscience / HyDrone",
            .speedKnotsMin     = 2.0f,
            .speedKnotsMax     = 5.0f,
            .emissionFreqLowHz = 200000,
            .emissionFreqHighHz = 400000,
            .shallowWaterCapable = true,
            .isAerial          = false,
            .isSurfaceVessel   = false,
            .unitCostMin       = 15000.0f,
            .unitCostMax       = 45000.0f,
            .stepDelay         = 3
        };
    }
    
    if (type == "blueboat") {
        return {
            .agentType         = "blueboat",
            .manufacturer      = "Blue Robotics",
            .speedKnotsMin     = 2.0f,
            .speedKnotsMax     = 6.0f,
            .emissionFreqLowHz = 450000,
            .emissionFreqHighHz = 650000,
            .shallowWaterCapable = true,
            .isAerial          = false,
            .isSurfaceVessel   = true,
            .unitCostMin       = 5000.0f,
            .unitCostMax       = 5000.0f,
            .stepDelay         = 2
        };
    }
    
    if (type == "yuco") {
        return {
            .agentType         = "yuco",
            .manufacturer      = "Seaber",
            .speedKnotsMin     = 2.0f,
            .speedKnotsMax     = 6.0f,
            .emissionFreqLowHz = 300000,
            .emissionFreqHighHz = 600000,
            .shallowWaterCapable = true,
            .isAerial          = false,
            .isSurfaceVessel   = false,
            .unitCostMin       = 50000.0f,
            .unitCostMax       = 100000.0f,
            .stepDelay         = 2
        };
    }
    
    if (type == "nemosens") {
        return {
            .agentType         = "nemosens",
            .manufacturer      = "RTSYS",
            .speedKnotsMin     = 2.0f,
            .speedKnotsMax     = 4.0f,
            .emissionFreqLowHz = 200000,
            .emissionFreqHighHz = 500000,
            .shallowWaterCapable = true,
            .isAerial          = false,
            .isSurfaceVessel   = false,
            .unitCostMin       = 60000.0f,
            .unitCostMax       = 115000.0f,
            .stepDelay         = 3
        };
    }
    
    if (type == "hugin") {
        return {
            .agentType         = "hugin",
            .manufacturer      = "Kongsberg Maritime",
            .speedKnotsMin     = 2.0f,
            .speedKnotsMax     = 5.0f,
            .emissionFreqLowHz = 200000,
            .emissionFreqHighHz = 400000,
            .shallowWaterCapable = false,
            .isAerial          = false,
            .isSurfaceVessel   = false,
            .unitCostMin       = 2000000.0f,
            .unitCostMax       = 4000000.0f,
            .stepDelay         = 3
        };
    }
    
    // ════════════════════════════════════════════════════════════════════════════════
    //  UAVs (Unmanned Aerial Vehicles) — not detectable by sonar
    // ════════════════════════════════════════════════════════════════════════════════
    
    if (type == "tb2") {
        return {
            .agentType         = "tb2",
            .manufacturer      = "Bayraktar",
            .speedKnotsMin     = 90.0f,
            .speedKnotsMax     = 110.0f,
            .emissionFreqLowHz = 0,
            .emissionFreqHighHz = 0,
            .shallowWaterCapable = false,
            .isAerial          = true,
            .isSurfaceVessel   = false,
            .unitCostMin       = 2000000.0f,
            .unitCostMax       = 5000000.0f,
            .stepDelay         = 1
        };
    }
    
    if (type == "queenhornet") {
        return {
            .agentType         = "queenhornet",
            .manufacturer      = "Zala Aero",
            .speedKnotsMin     = 38.0f,
            .speedKnotsMax     = 43.0f,
            .emissionFreqLowHz = 0,
            .emissionFreqHighHz = 0,
            .shallowWaterCapable = false,
            .isAerial          = true,
            .isSurfaceVessel   = false,
            .unitCostMin       = 1000.0f,
            .unitCostMax       = 5000.0f,
            .stepDelay         = 1
        };
    }
    
    if (type == "shahed") {
        return {
            .agentType         = "shahed",
            .manufacturer      = "Iranian Unmanned Aircraft",
            .speedKnotsMin     = 90.0f,
            .speedKnotsMax     = 100.0f,
            .emissionFreqLowHz = 0,
            .emissionFreqHighHz = 0,
            .shallowWaterCapable = false,
            .isAerial          = true,
            .isSurfaceVessel   = false,
            .unitCostMin       = 20000.0f,
            .unitCostMax       = 50000.0f,
            .stepDelay         = 1
        };
    }
    
    // Unknown type
    throw std::invalid_argument("Unknown vehicle type: " + type);
}
