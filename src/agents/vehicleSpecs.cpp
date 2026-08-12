#include "vehicleSpecs.h"
#include <stdexcept>
#include <unordered_map>
#include <string_view>
#include <array>

// ════════════════════════════════════════════════════════════════════════════════
//  VEHICLE SPECS DATABASE
// ════════════════════════════════════════════════════════════════════════════════
//
//  Central registry of all supported UUV/UAV types.
//  O(1) lookup via unordered_map — far faster and more maintainable
//  than the previous if-else chain.
//
//  Categories:
//    UUVs  (underwater)      — detectable by hydrophone
//    USVs  (surface vessels) — NOT detectable by hydrophone
//    UAVs  (aerial)          — NOT detectable by hydrophone
//
// ════════════════════════════════════════════════════════════════════════════════

namespace {

// Helper to build a specs entry concisely
[[nodiscard]] VehicleSpecs makeSpecs(
    std::string_view agentType,
    std::string_view manufacturer,
    float speedMinKn, float speedMaxKn,
    int freqLowHz, int freqHighHz,
    bool shallowCapable,
    bool aerial,
    bool surfaceVessel,
    float costMin, float costMax,
    int delay) noexcept
{
    VehicleSpecs s;
    s.agentType            = std::string(agentType);
    s.manufacturer         = std::string(manufacturer);
    s.speedKnotsMin        = speedMinKn;
    s.speedKnotsMax        = speedMaxKn;
    s.emissionFreqLowHz    = freqLowHz;
    s.emissionFreqHighHz   = freqHighHz;
    s.shallowWaterCapable  = shallowCapable;
    s.isAerial             = aerial;
    s.isSurfaceVessel      = surfaceVessel;
    s.unitCostMin          = costMin;
    s.unitCostMax          = costMax;
    s.stepDelay            = delay;
    return s;
}

// The one true spec registry — add new vehicles here
const auto& getSpecRegistry() {
    static const std::unordered_map<std::string, VehicleSpecs> registry = {
        // ══════════════════════════════════════════════════════════════════════
        //  UUVs  (Underwater Unmanned Vehicles) — hydrophone-detectable
        // ══════════════════════════════════════════════════════════════════════
        {"bluerov2", makeSpecs(
            "bluerov2", "Blue Robotics",
            1.0f, 3.0f,
            300000, 450000,
            /*shallow=*/true, /*aerial=*/false, /*surface=*/false,
            6000.0f, 6000.0f,
            4  /* 1-3 kn → slowest */)},

        {"riptide", makeSpecs(
            "riptide", "Oceanscience / HyDrone",
            2.0f, 5.0f,
            200000, 400000,
            /*shallow=*/true, /*aerial=*/false, /*surface=*/false,
            15000.0f, 45000.0f,
            3  /* 2-5 kn → medium */)},

        {"yuco", makeSpecs(
            "yuco", "Seaber",
            2.0f, 6.0f,
            300000, 600000,
            /*shallow=*/true, /*aerial=*/false, /*surface=*/false,
            50000.0f, 100000.0f,
            2  /* 2-6 kn → fast */)},

        {"nemosens", makeSpecs(
            "nemosens", "RTSYS",
            2.0f, 4.0f,
            200000, 500000,
            /*shallow=*/true, /*aerial=*/false, /*surface=*/false,
            60000.0f, 115000.0f,
            3  /* 2-4 kn → medium */)},

        {"hugin", makeSpecs(
            "hugin", "Kongsberg Maritime",
            2.0f, 5.0f,
            200000, 400000,
            /*shallow=*/true, /*aerial=*/false, /*surface=*/false,
            2000000.0f, 4000000.0f,
            3  /* 2-5 kn → medium */)},

        // ══════════════════════════════════════════════════════════════════════
        //  Reference baseline AUV (team planning doc: "3 Anduril Dive-LD AUVs
        //  come from three different directions"). The baseline attacker every
        //  defender configuration is tested against for GA fitness evaluation.
        //
        //  ⚠ SPEC ESTIMATES: Anduril does not publish exact Dive-LD figures.
        //  Values below are conservative estimates consistent with a long-
        //  endurance, low-signature mine-countermeasure AUV, and are marked
        //  accordingly. They are reasonable placeholders, NOT sourced figures.
        //  - speed:   long-endurance platforms trade speed for endurance (≈1-3 kn)
        //  - cost:    marketed as a low-cost HUGIN alternative; est. $0.5M-$1.0M
        //  - acoustic: plausible UUV emission band, consistent with hugin/yuco
        //  ══════════════════════════════════════════════════════════════════════
        {"diveld", makeSpecs(
            "diveld", "Anduril Industries",
            1.0f, 3.0f,             // ESTIMATE: slow long-endurance AUV
            200000, 400000,         // ESTIMATE: plausible UUV emission band
            /*shallow=*/true, /*aerial=*/false, /*surface=*/false,
            500000.0f, 1000000.0f,  // ESTIMATE: low-cost HUGIN alternative
            4  /* 1-3 kn → slowest, like bluerov2 */)},

        // ══════════════════════════════════════════════════════════════════════
        //  USVs  (Unmanned Surface Vessels) — NOT hydrophone-detectable
        // ══════════════════════════════════════════════════════════════════════
        {"blueboat", makeSpecs(
            "blueboat", "Blue Robotics",
            2.0f, 6.0f,
            450000, 650000,
            /*shallow=*/true, /*aerial=*/false, /*surface=*/true,
            5000.0f, 5000.0f,
            2  /* 2-6 kn → fast */)},

        // ══════════════════════════════════════════════════════════════════════
        //  UAVs  (Unmanned Aerial Vehicles) — NOT hydrophone-detectable
        // ══════════════════════════════════════════════════════════════════════
        {"tb2", makeSpecs(
            "tb2", "Bayraktar",
            90.0f, 110.0f,
            0, 0,  // no acoustic emission
            /*shallow=*/false, /*aerial=*/true, /*surface=*/false,
            2000000.0f, 5000000.0f,
            1  /* 90-110 kn → fastest */)},

        {"queenhornet", makeSpecs(
            "queenhornet", "Zala Aero",
            38.0f, 43.0f,
            0, 0,  // no acoustic emission
            /*shallow=*/false, /*aerial=*/true, /*surface=*/false,
            1000.0f, 5000.0f,
            1  /* 38-43 kn → fastest */)},

        {"shahed", makeSpecs(
            "shahed", "Iranian Unmanned Aircraft",
            90.0f, 100.0f,
            0, 0,  // no acoustic emission
            /*shallow=*/false, /*aerial=*/true, /*surface=*/false,
            20000.0f, 50000.0f,
            1  /* 90-100 kn → fastest */)},
    };
    return registry;
}

} // anonymous namespace

// ════════════════════════════════════════════════════════════════════════════════
//  PUBLIC API
// ════════════════════════════════════════════════════════════════════════════════

VehicleSpecs getVehicleSpecs(const std::string& type) {
    const auto& registry = getSpecRegistry();
    auto it = registry.find(type);
    if (it != registry.end()) {
        return it->second;
    }
    throw std::invalid_argument("Unknown vehicle type: " + type);
}
