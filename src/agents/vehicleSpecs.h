#ifndef VEHICLE_SPECS_H
#define VEHICLE_SPECS_H

#include <string>
#include <string_view>
#include <cstdint>

// ═══════════════════════════════════════════════════════════════════════════════
//  VehicleSpecs — Real-World UUV/UAV Parameter Registry
// ═══════════════════════════════════════════════════════════════════════════════
//
//  This is the SINGLE SOURCE OF TRUTH for all vehicle parameters used
//  throughout the simulation. Every agent type's speed, cost, acoustic
//  emission frequency, and operational capabilities are defined here.
//
//  Design Decisions:
//  ────────────────
//  1. Single Registry: All vehicle specs live in one place (vehicleSpecs.cpp).
//     Adding a new vehicle type requires only adding a new map entry — no
//     other code changes needed.
//
//  2. Hydrophone Detectability: Derived from two boolean flags (isAerial,
//     isSurfaceVessel). Underwater UUVs are detectable; aerial and surface
//     vessels are not.
//
//  3. Frequency-Based Detection: The frequency range (emissionFreqLowHz /
//     emissionFreqHighHz) models real acoustic signatures. A detector can
//     only track agents whose emission frequency overlaps its sensing band.
//
//  4. Speed Simulation: stepDelay approximates real knot-speed differences
//     on the grid. stepDelay=1 (fastest, e.g. TB2 at 90-110 kn) moves every
//     step; stepDelay=4 (slowest, e.g. BlueROV2 at 1-3 kn) moves every 4th.
//
//  Category Summary:
//  ────────────────
//  Category | Detectable? | Examples                  | stepDelay
//  ─────────┼─────────────┼───────────────────────────┼──────────
//  UUV      | Yes         | BlueROV2, HUGIN, Riptide  | 2-4
//  USV      | No          | BlueBoat                  | 2
//  UAV      | No          | TB2, Shahed, QueenHornet  | 1
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @struct VehicleSpecs
 * @brief Complete parameter set for a single vehicle type.
 *
 * All fields are populated by getVehicleSpecs() which reads from an
 * O(1) unordered_map registry in vehicleSpecs.cpp.
 */
struct VehicleSpecs {
    // ── Identity ──────────────────────────────────────────────────────────────
    std::string agentType;              ///< Canonical type key (e.g., "bluerov2", "tb2")
    std::string manufacturer;           ///< Manufacturer name (e.g., "Blue Robotics")

    // ── Speed ─────────────────────────────────────────────────────────────────
    float speedKnotsMin;                ///< Minimum operating speed (knots)
    float speedKnotsMax;                ///< Maximum operating speed (knots)

    // ── Acoustic Emission ─────────────────────────────────────────────────────
    int emissionFreqLowHz;              ///< Lower bound of acoustic emission (Hz)
    int emissionFreqHighHz;             ///< Upper bound of acoustic emission (Hz)

    // ── Capability Flags ──────────────────────────────────────────────────────
    bool shallowWaterCapable;           ///< Can operate in shallow/harbor waters
    bool isAerial;                      ///< True = UAV (not sonar-detectable)
    bool isSurfaceVessel;               ///< True = USV (not sonar-detectable)

    // ── Cost ──────────────────────────────────────────────────────────────────
    float unitCostMin;                  ///< Minimum unit cost (USD)
    float unitCostMax;                  ///< Maximum unit cost (USD)

    // ── Simulation ────────────────────────────────────────────────────────────
    int stepDelay;                      ///< Grid steps between moves (speed proxy)

    // ── Computed Properties ───────────────────────────────────────────────────

    /**
     * @brief Check if this vehicle is detectable by underwater hydrophone.
     * @return true only for underwater UUVs (not aerial, not surface)
     */
    [[nodiscard]] bool isDetectableByHydrophone() const noexcept {
        return !isAerial && !isSurfaceVessel;
    }

    /**
     * @brief Get a short code/abbreviation for the vehicle type.
     * @return e.g., "BR" for BlueROV2, "T2" for TB2
     */
    [[nodiscard]] std::string_view shortCode() const noexcept {
        // Map common types to 2-char codes
        if (agentType == "bluerov2")    return "BR";
        if (agentType == "riptide")     return "RP";
        if (agentType == "blueboat")    return "BB";
        if (agentType == "yuco")        return "YU";
        if (agentType == "nemosens")    return "NS";
        if (agentType == "hugin")       return "HU";
        if (agentType == "diveld")      return "DV";
        if (agentType == "tb2")         return "T2";
        if (agentType == "queenhornet") return "QH";
        if (agentType == "shahed")      return "SH";
        return "??";
    }

    /**
     * @brief Get the cost category as a string.
     * @return "budget" (< $10k), "mid-range" ($10k-$100k), "premium" ($100k-$1M), "flagship" (> $1M)
     */
    [[nodiscard]] std::string_view costCategory() const noexcept {
        float avgCost = (unitCostMin + unitCostMax) / 2.0f;
        if (avgCost < 10000.0f)       return "budget";
        if (avgCost < 100000.0f)      return "mid-range";
        if (avgCost < 1000000.0f)     return "premium";
        return "flagship";
    }
};

/**
 * @brief Retrieve vehicle specs for a given type string.
 * @param type  Canonical type key (e.g., "bluerov2", "hugin", "tb2")
 * @return Fully populated VehicleSpecs struct
 *
 * @throws std::invalid_argument If type is not found in the registry.
 *
 * Thread safety: The underlying registry is initialized once (static local)
 * and is safe to call from multiple threads after the first call.
 *
 * Performance: O(1) average case via unordered_map lookup.
 *
 * Usage:
 *   VehicleSpecs specs = getVehicleSpecs("hugin");
 *   float cost = specs.unitCostMax;  // $4,000,000
 *   if (specs.isDetectableByHydrophone()) { ... }
 */
VehicleSpecs getVehicleSpecs(const std::string& type);

#endif // VEHICLE_SPECS_H

