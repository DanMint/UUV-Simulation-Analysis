/**
 * test_stress.cpp
 *
 * Stress test: runs a large simulation to verify stability and performance.
 * Creates a scenario with many agents and runs for many steps.
 *
 * Exit codes:
 *   0 = PASS
 *   1 = FAIL
 */

#include "mapCreation.h"
#include "spawnConfig.h"
#include "simulation.h"
#include "simResult.h"

#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>
#include <chrono>
#include <cassert>

static MapCreation buildStressMap(int cellsN = 200) {
    std::vector<std::vector<int>> grid(cellsN, std::vector<int>(cellsN, 0));
    for (int r = 0; r < cellsN; r++) {
        for (int c = 0; c < cellsN; c++) {
            grid[r][c] = ((r + c) % 7 == 0) ? 1 : 0;
        }
    }
    return MapCreation::fromGridData(grid, cellsN, 800, 800);
}

static SpawnConfig buildStressConfig(MapCreation& map, int nSeekers, int nTargets,
                                     int nDetectors, int nInterceptors) {
    SpawnConfig config;
    MapInfo info;
    info.shpPath = "(stress test)";
    info.cellsN = map.getCellsN();
    info.canvasWidth = map.getCanvasWidth();
    info.canvasHeight = map.getCanvasHeight();
    config.setMapData(info, map.getGrid());

    auto addUnit = [&](const std::string& type, int row, int col) {
        config.addUnit(type, row, col);
    };

    for (int i = 0; i < nSeekers; i++) {
        addUnit("seeker", 2 + i % 10, 2 + (i / 10) % 10);
    }
    for (int i = 0; i < nTargets; i++) {
        addUnit("target", map.getCellsN() - 5 - i % 10, map.getCellsN() - 5 - (i / 10) % 10);
    }
    for (int i = 0; i < nDetectors; i++) {
        addUnit("detector", map.getCellsN() / 2 + i % 5, map.getCellsN() / 2 + (i / 5) % 5);
    }
    for (int i = 0; i < nInterceptors; i++) {
        addUnit("interceptor", map.getCellsN() / 2 + i % 5, map.getCellsN() / 2 + 3 + (i / 5) % 5);
    }

    return config;
}

int main() {
    const int cellsN = 200;
    const int nSeekers = 20;
    const int nTargets = 10;
    const int nDetectors = 5;
    const int nInterceptors = 5;
    const int maxSteps = 1000;

    std::cout << "\n=== Stress Test ===\n";
    std::cout << "Map: " << cellsN << "x" << cellsN << "\n";
    std::cout << "Agents: " << nSeekers << " seekers, " << nTargets << " targets, "
              << nDetectors << " detectors, " << nInterceptors << " interceptors\n";
    std::cout << "Max steps: " << maxSteps << "\n\n";

    auto map = buildStressMap(cellsN);
    auto config = buildStressConfig(map, nSeekers, nTargets, nDetectors, nInterceptors);

    auto start = std::chrono::high_resolution_clock::now();
    Simulation sim(map, config, maxSteps, 42);
    SimResult result = sim.run();
    auto end = std::chrono::high_resolution_clock::now();

    double wallMs = std::chrono::duration<double, std::milli>(end - start).count();
    double stepsPerSec = (result.totalSteps > 0) ? (result.totalSteps / (wallMs / 1000.0)) : 0.0;

    std::cout << "Results:\n";
    std::cout << "  Total steps:       " << result.totalSteps << "\n";
    std::cout << "  Targets destroyed: " << result.targetsDestroyed << " / " << result.targetResults.size() << "\n";
    std::cout << "  Seekers reached:   " << result.seekersThatReached << " / " << result.seekerResults.size() << "\n";
    std::cout << "  Wall time:         " << wallMs << " ms\n";
    std::cout << "  Throughput:        " << static_cast<int>(stepsPerSec) << " steps/sec\n";

    bool pass = result.totalSteps > 0 && result.totalSteps <= maxSteps;
    if (!pass) {
        std::cerr << "FAIL: invalid step count\n";
        return 1;
    }

    std::cout << "\n[PASS] Stress test completed\n";
    return 0;
}
