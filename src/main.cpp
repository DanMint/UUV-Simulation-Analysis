#include "mapCreation.h"
#include "spawnConfig.h"
#include "mapVisualizer.h"
#include "simulation.h"
#include <iostream>
#include <string>

void printUsage(const char* progName) {
    std::cout << "Usage:\n"
              << "  " << progName << " <shapefile.shp> [cells_n]\n"
              << "  " << progName << " --cache <cache_file.txt>\n"
              << "  " << progName << " --scenario <scenario.json>\n"
              << "\nSpawn Tool Controls:\n"
              << "  Left click   - Place unit on water cell\n"
              << "  Right click  - Remove unit\n"
              << "  S key        - Switch to Seeker mode (attacker, red triangle)\n"
              << "  T key        - Switch to Target mode (defender, blue square)\n"
              << "  C key        - Clear all units\n"
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

            if (config.totalUnits() == 0) {
                std::cout << "No units placed. Exiting.\n";
                return 0;
            }

            // Stamp units onto grid
            for (const auto& unit : config.getUnits()) {
                int unitType = MapCreation::WATER;
                if (unit.type == "seeker")  unitType = MapCreation::SEEKER;
                if (unit.type == "target")  unitType = MapCreation::TARGET;
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
                if (unit.type == "seeker")  unitType = MapCreation::SEEKER;
                if (unit.type == "target")  unitType = MapCreation::TARGET;
                map.placeUnit(unit.row, unit.col, unitType);
            }
        }

        config.printSummary();
        map.printGrid();

        // ── Run simulation ───────────────────────────────────────────

        if (config.countType("seeker") == 0) {
            std::cout << "No seekers placed. Cannot run simulation.\n";
            return 0;
        }
        if (config.countType("target") == 0) {
            std::cout << "No targets placed. Cannot run simulation.\n";
            return 0;
        }

        Simulation sim(map, config, 2000);
        SimResult result = sim.run();
        result.print();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}