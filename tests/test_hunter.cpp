#include "mapCreation.h"
#include "spawnConfig.h"
#include "simulation.h"
#include "simResult.h"

#include <iostream>
#include <stdexcept>

int main() {
    try {
        MapCreation map = MapCreation::fromCache("tests/fixtures/grid_cache.txt");

        SpawnConfig config;
        config.addUnit("hunter", 31, 75);
        config.addUnit("seeker", 32, 76);
        config.addUnit("target", 30, 74);

        for (const auto& unit : config.getUnits()) {
            int type = MapCreation::WATER;
            if (unit.type == "seeker") type = MapCreation::SEEKER;
            if (unit.type == "target") type = MapCreation::TARGET;
            if (unit.type == "hunter") type = MapCreation::HUNTER;
            if (!map.placeUnit(unit.row, unit.col, type)) {
                throw std::runtime_error("Failed to place unit at (" + std::to_string(unit.row) + "," + std::to_string(unit.col) + ")");
            }
        }

        Simulation sim(map, config, 1);
        SimResult result = sim.run();

        if (result.seekerResults.empty()) {
            std::cerr << "ERROR: no seeker results were produced" << std::endl;
            return 2;
        }

        const auto& hunter = result.hunterResults.front();
        if (!hunter.capturedSeeker) {
            std::cerr << "Expected the seeker to be intercepted by the hunter, but it was not." << std::endl;
            return 1;
        }

        // Second regression: the hunter should also capture the seeker if they meet
        // after both move on the same turn.
        MapCreation crossingMap = MapCreation::fromCache("tests/fixtures/grid_cache.txt");
        SpawnConfig crossingConfig;
        crossingConfig.addUnit("hunter", 31, 75);
        crossingConfig.addUnit("seeker", 31, 77);
        crossingConfig.addUnit("target", 31, 76);

        for (const auto& unit : crossingConfig.getUnits()) {
            int type = MapCreation::WATER;
            if (unit.type == "seeker") type = MapCreation::SEEKER;
            if (unit.type == "target") type = MapCreation::TARGET;
            if (unit.type == "hunter") type = MapCreation::HUNTER;
            if (!crossingMap.placeUnit(unit.row, unit.col, type)) {
                throw std::runtime_error("Failed to place crossing unit at (" + std::to_string(unit.row) + "," + std::to_string(unit.col) + ")");
            }
        }

        Simulation crossingSim(crossingMap, crossingConfig, 1);
        SimResult crossingResult = crossingSim.run();
        if (crossingResult.hunterResults.empty()) {
            std::cerr << "ERROR: no hunter results were produced for the crossing scenario" << std::endl;
            return 2;
        }

        const auto& crossingHunter = crossingResult.hunterResults.front();
        if (!crossingHunter.capturedSeeker) {
            std::cerr << "Expected the hunter to capture the seeker after both agents moved on the same turn." << std::endl;
            return 1;
        }

        std::cout << "PASS: hunter captured a seeker." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 2;
    }
}
