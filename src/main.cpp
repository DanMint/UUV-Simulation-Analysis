#include "mapCreation.h"
#include "spawnConfig.h"
#include "mapVisualizer.h"
#include <iostream>
#include <string>

void printUsage(const char* progName) {
    std::cout << "Usage:\n"
              << "  " << progName << " <shapefile.shp> [cells_n]\n"
              << "  " << progName << " --cache <cache_file.txt>\n"
              << "\nSpawn Tool Controls:\n"
              << "  Left click   - Place unit on water cell\n"
              << "  Right click  - Remove unit\n"
              << "  S key        - Switch to Seeker mode (attacker, red triangle)\n"
              << "  T key        - Switch to Target mode (defender, blue square)\n"
              << "  C key        - Clear all units\n"
              << "  Enter        - Save scenario and close\n"
              << "  Escape       - Close without saving\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    try {
        // ── Step 1: Load the map ─────────────────────────────────────

        MapCreation* mapPtr = nullptr;
        std::string shpPath = "";
        std::string firstArg = argv[1];

        if (firstArg == "--cache") {
            if (argc < 3) {
                std::cerr << "Error: --cache requires a file path\n";
                return 1;
            }
            std::cout << "Loading grid from cache...\n";
            static MapCreation cachedMap = MapCreation::fromCache(argv[2]);
            mapPtr = &cachedMap;
            shpPath = "(from cache: " + std::string(argv[2]) + ")";
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

        // ── Step 2: Open the spawn tool ──────────────────────────────

        std::cout << "Opening spawn tool...\n";
        std::cout << "Place your units, then press Enter to save.\n\n";

        MapVisualizer visualizer(map);
        SpawnConfig config = visualizer.run(""); // don't save inside visualizer

        if (config.totalUnits() == 0) {
            std::cout << "No units placed. Exiting.\n";
            return 0;
        }

        // ── Step 3: Stamp units onto the grid ──────────────────────

        for (const auto& unit : config.getUnits()) {
            int unitType = MapCreation::WATER;
            if (unit.type == "seeker")  unitType = MapCreation::SEEKER;
            if (unit.type == "target")  unitType = MapCreation::TARGET;
            map.placeUnit(unit.row, unit.col, unitType);
        }

        // Print grid with units visible
        map.printGrid();

        // ── Step 4: Attach map data to the config ────────────────────

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

        // ── Step 5: Save the complete scenario ───────────────────────

        std::string savePath = "scenario.json";
        config.saveJSON(savePath);
        config.printSummary();

        std::cout << "Ready for simulation with "
                  << config.countType("seeker") << " seekers and "
                  << config.countType("target") << " targets.\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}