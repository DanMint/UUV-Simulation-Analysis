#include "mapCreation.h"
#include "spawnConfig.h"
#include "mapVisualizer.h"
#include "simulation.h"
#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>

void printUsage(const char* progName) {
    std::cout << "Usage:\n"
              << "  " << progName << " <shapefile.shp> [cells_n]\n"
              << "  " << progName << " --cache <cache_file.txt>\n"
              << "  " << progName << " --scenario <scenario.json>\n"
              << "\nSpawn Tool Controls:\n"
              << "  Left click   - Place unit on water cell\n"
              << "  Right click  - Remove unit\n"
              << "  S key        - Switch to Seeker mode (attacker, red triangle)\n"
              << "  H key        - Switch to Hunter mode (pursuer, green diamond)\n"
              << "  T key        - Switch to Target mode (defender, blue square)\n"
              << "  D key        - Switch to Detector mode (sensor, orange diamond)\n"
              << "  I key        - Switch to Interceptor mode (effector, purple diamond)\n"
              << "  Z key        - Draw ATTACKER spawn zones for the GA\n"
              << "  X key        - Draw DEFENDER spawn zones for the GA\n"
              << "  Q key        - Toggle GA-prep mode (targets + zones only)\n"
              << "  + / - keys   - Adjust detector sensing radius\n"
              << "  { / } keys   - Adjust interceptor kill radius\n"
              << "  [ / ] keys   - Adjust noise level (wave/wind)\n"
              << "  C key        - Clear all units (zones preserved)\n"
              << "  Enter        - Save scenario and run simulation\n"
              << "  Escape       - Close without saving\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    try {
        MapCreation* mapPtr = nullptr;
        SpawnConfig config;
        std::string shpPath = "";
        std::string firstArg = argv[1];
        bool needSpawnTool = true;

        // ── Load map ─────────────────────────────────────────────────

        if (firstArg == "--scenario") {
            // Load a complete scenario (map + grid + units)
            if (argc < 3) {
                std::cerr << "Error: --scenario requires a file path\n";
                return 1;
            }
            std::cout << "Loading scenario from " << argv[2] << "...\n";
            config = SpawnConfig::loadJSON(argv[2]);

            if (!config.hasMapData()) {
                std::cerr << "Error: scenario file has no map data\n";
                return 1;
            }

            // Build a MapCreation from the cached grid in the config
            const MapInfo& info = config.getMapInfo();
            static MapCreation scenarioMap = MapCreation::fromCache("grid_cache.txt");
            mapPtr = &scenarioMap;
            shpPath = info.shpPath;
            needSpawnTool = false;
        }
        else if (firstArg == "--cache") {
            if (argc < 3) {
                std::cerr << "Error: --cache requires a file path\n";
                return 1;
            }
            std::cout << "Loading grid from cache...\n";
            static MapCreation cachedMap = MapCreation::fromCache(argv[2]);
            mapPtr = &cachedMap;
            shpPath = "(from cache)";
        }
        else {
            int cellsN = (argc >= 3) ? std::stoi(argv[2]) : 100;
            shpPath = firstArg;
            std::cout << "Loading shapefile: " << firstArg << "\n";
            std::cout << "Grid resolution: " << cellsN << "x" << cellsN << "\n\n";

            static MapCreation shpMap(firstArg, cellsN);
            mapPtr = &shpMap;
            shpMap.saveCache("grid_cache.txt");
        }

        MapCreation& map = *mapPtr;
        map.printStats();

        // ── Spawn tool (if not loading a scenario) ───────────────────

        if (needSpawnTool) {
            std::cout << "Opening spawn tool...\n";
            std::cout << "Place your units, then press Enter to run simulation.\n\n";

            MapVisualizer visualizer(map);
            config = visualizer.run("");

            if (config.totalUnits() == 0 &&
                !config.hasAttackerZones() && !config.hasDefenderZones()) {
                std::cout << "No units or zones placed. Exiting.\n";
                return 0;
            }

            // Stamp units onto grid
            for (const auto& unit : config.getUnits()) {
                int unitType = MapCreation::WATER;
                if (unit.type == "seeker")   unitType = MapCreation::SEEKER;
                if (unit.type == "hunter")   unitType = MapCreation::HUNTER;
                if (unit.type == "target")   unitType = MapCreation::TARGET;
                if (unit.type == "detector") unitType = MapCreation::DETECTOR;
                if (unit.type == "interceptor") unitType = MapCreation::INTERCEPTOR;
                map.placeUnit(unit.row, unit.col, unitType);
            }

            // Attach map data and save scenario
            MapInfo info;
            info.shpPath      = shpPath;
            info.cellsN       = map.getCellsN();
            info.canvasWidth   = map.getCanvasWidth();
            info.canvasHeight  = map.getCanvasHeight();
            info.minDepth     = map.getMinDepth();
            info.maxDepth     = map.getMaxDepth();
            info.waterCount   = map.getWaterCount();
            info.landCount    = map.getLandCount();
            config.setMapData(info, map.getGrid());
            config.saveJSON("scenario.json");
        }
        else {
            // Stamp units from loaded scenario onto grid
            for (const auto& unit : config.getUnits()) {
                int unitType = MapCreation::WATER;
                if (unit.type == "seeker")   unitType = MapCreation::SEEKER;
                if (unit.type == "hunter")   unitType = MapCreation::HUNTER;
                if (unit.type == "target")   unitType = MapCreation::TARGET;
                if (unit.type == "detector") unitType = MapCreation::DETECTOR;
                if (unit.type == "interceptor") unitType = MapCreation::INTERCEPTOR;
                map.placeUnit(unit.row, unit.col, unitType);
            }
        }

        config.printSummary();
        map.printGrid();

        // ── Run simulation ───────────────────────────────────────────

        // ── GA-prep scenario: no seekers placed but zones exist ──────
        // This is intentional: the user is preparing a scenario for the
        // Python GA to fill in. Save and exit cleanly — no simulation runs.
        if (config.countType("seeker") == 0 && config.hasAttackerZones()) {
            std::cout << "\n=== GA Preparation Scenario ===\n";
            std::cout << "  Targets:         " << config.countType("target") << "\n";
            std::cout << "  Detectors:       " << config.countType("detector") << "\n";
            std::cout << "  Interceptors:    " << config.countType("interceptor") << "\n";
            std::cout << "  Attacker zones:  " << config.getAttackerZones().size() << "\n";
            std::cout << "  Defender zones:  " << config.getDefenderZones().size() << "\n";
            std::cout << "\nScenario saved to scenario.json — no simulation run.\n";
            std::cout << "Hand this scenario to the Python GA to optimise attacker positions.\n";

            // Create empty runs/ so downstream visualize.py doesn't crash
            std::system("mkdir -p runs");
            return 0;
        }

        if (config.countType("seeker") == 0) {
            std::cout << "No seekers placed. Cannot run simulation.\n";
            return 0;
        }
        if (config.countType("target") == 0) {
            std::cout << "No targets placed. Cannot run simulation.\n";
            return 0;
        }

        // ── Iteration setup ──────────────────────────────────────────

        int numIterations = 1;
        double noiseIncrement = 0.0;
        double startingNoise = config.getMaxNoiseLevel();

        std::cout << "\n── Iteration Configuration ──\n";
        std::cout << "Starting noise level: " << startingNoise << "\n";
        std::cout << "Number of iterations (1 = single run): ";
        std::cin >> numIterations;
        if (numIterations < 1) numIterations = 1;

        if (numIterations > 1) {
            std::cout << "Noise increment per iteration: ";
            std::cin >> noiseIncrement;
            if (noiseIncrement < 0.0) noiseIncrement = 0.0;
        }

        // Create runs/ directory
        std::string runsDir = "runs";
        std::system(("mkdir -p " + runsDir).c_str());

        std::cout << "\n── Running " << numIterations << " iteration(s) ──\n";
        if (numIterations > 1) {
            std::cout << "  Noise range: " << startingNoise
                      << " → " << (startingNoise + noiseIncrement * (numIterations - 1))
                      << " (step " << noiseIncrement << ")\n";
        }
        std::cout << "  Results will be saved to " << runsDir << "/\n\n";

        // ── Iteration loop ───────────────────────────────────────────

        for (int iter = 0; iter < numIterations; iter++) {
            double currentNoise = startingNoise + noiseIncrement * iter;
            config.setMaxNoiseLevel(currentNoise);

            std::cout << "\n════════════════════════════════════════════\n";
            std::cout << "  Iteration " << (iter + 1) << " / " << numIterations
                      << "  |  Noise: " << currentNoise << "\n";
            std::cout << "════════════════════════════════════════════\n";

            // Reset the grid: clear all units, then re-stamp them
            // This ensures each iteration starts from the same state
            map.clearAllUnits();
            for (const auto& unit : config.getUnits()) {
                int unitType = MapCreation::WATER;
                if (unit.type == "seeker")   unitType = MapCreation::SEEKER;
                if (unit.type == "hunter")   unitType = MapCreation::HUNTER;
                if (unit.type == "target")   unitType = MapCreation::TARGET;
                if (unit.type == "detector") unitType = MapCreation::DETECTOR;
                if (unit.type == "interceptor") unitType = MapCreation::INTERCEPTOR;
                map.placeUnit(unit.row, unit.col, unitType);
            }

            // Run the simulation
            Simulation sim(map, config, 2000);
            SimResult result = sim.run();
            result.print();

            // Build filename from noise level: e.g. "runs/0.5.json"
            // Use a consistent format to avoid floating point weirdness
            std::ostringstream filename;
            filename << runsDir << "/";

            // Format noise level: remove trailing zeros for clean filenames
            std::ostringstream noiseStr;
            noiseStr << currentNoise;
            std::string noiseLabel = noiseStr.str();

            filename << noiseLabel << ".json";
            result.saveJSON(filename.str());
        }

        // ── Summary ──────────────────────────────────────────────────

        if (numIterations > 1) {
            std::cout << "\n════════════════════════════════════════════\n";
            std::cout << "  All " << numIterations << " iterations complete.\n";
            std::cout << "  Results saved to " << runsDir << "/\n";
            std::cout << "════════════════════════════════════════════\n\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}