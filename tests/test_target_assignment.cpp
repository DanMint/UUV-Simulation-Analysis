#include "mapCreation.h"
#include "spawnConfig.h"
#include "simulation.h"
#include "simResult.h"

#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

int main() {
    try {
        MapCreation map = MapCreation::fromCache("tests/fixtures/grid_cache.txt");

        SpawnConfig config;
        config.addUnit("seeker", 32, 76);
        config.addUnit("seeker", 31, 75);
        config.addUnit("target", 30, 74);
        config.addUnit("target", 13, 15);

        for (const auto& unit : config.getUnits()) {
            int type = MapCreation::WATER;
            if (unit.type == "seeker") type = MapCreation::SEEKER;
            if (unit.type == "target") type = MapCreation::TARGET;
            if (!map.placeUnit(unit.row, unit.col, type)) {
                throw std::runtime_error("Failed to place unit at (" + std::to_string(unit.row) + "," + std::to_string(unit.col) + ")");
            }
        }

        Simulation sim(map, config, 1);
        SimResult result = sim.run();

        std::vector<int> assignedTargets;
        for (const auto& seeker : result.seekerResults) {
            assignedTargets.push_back(seeker.targetId);
        }

        std::set<int> uniqueTargets(assignedTargets.begin(), assignedTargets.end());
        std::cout << "Assigned targets:";
        for (int targetId : assignedTargets) {
            std::cout << " " << targetId;
        }
        std::cout << "\n";

        if (uniqueTargets.size() < 2) {
            std::cerr << "Expected two different target assignments, but got "
                      << uniqueTargets.size() << " unique target(s).\n";
            return 1;
        }

        std::cout << "PASS: seekers were assigned to different targets when alternatives existed.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 2;
    }
}
