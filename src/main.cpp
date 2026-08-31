#include "mapCreation.h"
#include "spawnConfig.h"
#include "mapVisualizer.h"
#include "simulation.h"
#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <cstdlib>
#include <stdexcept>

void runGuiControlPanel(GuiControlState& state);


namespace {

int categoryToMapUnitType(const std::string& category) {
    if (category == "seeker") {
        return MapCreation::SEEKER;
    }
    if (category == "target") {
        return MapCreation::TARGET;
    }
    if (category == "detector") {
        return MapCreation::DETECTOR;
    }
    if (category == "interceptor") {
        return MapCreation::INTERCEPTOR;
    }

    throw std::invalid_argument("Unknown unit category: " + category);
}

void stampConfiguredUnits(MapCreation& map, const SpawnConfig& config) {
    for (const auto& unit : config.getUnits()) {
        map.placeUnit(
            unit.row,
            unit.col,
            categoryToMapUnitType(unit.category));
    }
}

} // namespace

void printUsage(const char* progName) {
    std::cout << "Usage:\n"
              << "  " << progName << " <shapefile.shp> [cells_n]\n"
              << "  " << progName << " --cache <cache_file.txt>\n"
              << "  " << progName << " --scenario <scenario.json>\n"
              << "\nSpawn Tool Controls:\n"
              << "  Left click   - Place unit on water cell\n"
              << "  Right click  - Remove unit\n"
              << "  S key        - Select Seeker category (attacker, red triangle)\n"
              << "  T key        - Select Target category (defender, blue square)\n"
              << "  D key        - Select Detector category (sensor, orange diamond)\n"
              << "  I key        - Select Interceptor category (effector, purple diamond)\n"
              << "  B key        - Select Basic type\n"
              << "  F key        - Select Fast type (Seeker category only)\n"
              << "  E key        - Select Evader type (Seeker category only)\n"
              << "  M key        - Select Medium type (Detector or Interceptor)\n"
              << "  A key        - Select Advanced type (Detector or Interceptor)\n"
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
    // receive arguments from user
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    GuiControlState guiControl;
    auto shutdownGui = [&]() { guiControl.exitRequested.store(true); };

    try {
        std::unique_ptr<MapCreation> mapPtr;
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
            mapPtr = std::make_unique<MapCreation>(MapCreation::fromCache("grid_cache.txt"));
            shpPath = info.shpPath;
            needSpawnTool = false;
        }
        else if (firstArg == "--cache") {
            if (argc < 3) {
                std::cerr << "Error: --cache requires a file path\n";
                return 1;
            }
            std::cout << "Loading grid from cache...\n";
            mapPtr = std::make_unique<MapCreation>(MapCreation::fromCache(argv[2]));
            shpPath = "(from cache)";
        }
        else {
            int cellsInARow = (argc >= 3) ? std::stoi(argv[2]) : 100;
            shpPath = firstArg;
            std::cout << "Loading shapefile: " << firstArg << "\n";
            std::cout << "Grid resolution: " << cellsInARow << "x" << cellsInARow << "\n\n";

            mapPtr = std::make_unique<MapCreation>(firstArg, cellsInARow);
            mapPtr->saveCache("grid_cache.txt");
        }

        mapPtr->printStats();

        // ── Spawn tool (if not loading a scenario) ───────────────────

        if (needSpawnTool) {
            std::cout << "Opening spawn tool...\n";
            std::cout << "Place your units, then press Enter to run simulation.\n\n";

            while (true) {
                MapVisualizer visualizerWithGui(*mapPtr, 700, &guiControl);
                config = visualizerWithGui.run("");

                if (!guiControl.mapReloadRequested.exchange(false)) {
                    break;
                }

                const int cellsInARow = mapPtr->getCellsN();
                shpPath = guiControl.selectedMapPath;
                std::cout << "Loading selected map: " << shpPath << "\n";
                mapPtr = std::make_unique<MapCreation>(shpPath, cellsInARow);
                mapPtr->saveCache("grid_cache.txt");
                mapPtr->printStats();
                config.clear();
            }
            if (config.totalUnits() == 0 &&
                !config.hasAttackerZones() && !config.hasDefenderZones()) {
                shutdownGui();
            }

            if (config.totalUnits() == 0 &&
                !config.hasAttackerZones() && !config.hasDefenderZones()) {
                std::cout << "No units or zones placed. Exiting.\n";
                return 0;
            }

            // Stamp units onto grid. The grid cares about the broad category;
            // the concrete type is used later by category-specific behavior.
            stampConfiguredUnits(*mapPtr, config);

            // Attach map data and save scenario
            MapInfo info;
            info.shpPath      = shpPath;
            info.cellsInARow       = mapPtr->getCellsN();
            info.canvasWidth   = mapPtr->getCanvasWidth();
            info.canvasHeight  = mapPtr->getCanvasHeight();
            info.minDepth     = mapPtr->getMinDepth();
            info.maxDepth     = mapPtr->getMaxDepth();
            info.waterCount   = mapPtr->getWaterCount();
            info.landCount    = mapPtr->getLandCount();
            config.setMapData(info, mapPtr->getGrid());
            config.saveJSON("scenario.json");
        }
        else {
            // Stamp units from the loaded scenario onto the grid.
            stampConfiguredUnits(*mapPtr, config);
        }

        MapCreation& map = *mapPtr;

        config.printSummary();
        map.printGrid();

        // ── Run simulation ───────────────────────────────────────────

        // ── GA-prep scenario: no seekers placed but zones exist ──────
        // This is intentional: the user is preparing a scenario for the
        // Python GA to fill in. Save and exit cleanly — no simulation runs.
        if (config.countCategory("seeker") == 0 && config.hasAttackerZones()) {
            std::cout << "\n=== GA Preparation Scenario ===\n";
            std::cout << "  Targets:         " << config.countCategory("target") << "\n";
            std::cout << "  Detectors:       " << config.countCategory("detector") << "\n";
            std::cout << "  Interceptors:    " << config.countCategory("interceptor") << "\n";
            std::cout << "  Attacker zones:  " << config.getAttackerZones().size() << "\n";
            std::cout << "  Defender zones:  " << config.getDefenderZones().size() << "\n";
            std::cout << "\nScenario saved to scenario.json — no simulation run.\n";
            std::cout << "Hand this scenario to the Python GA to optimise attacker positions.\n";

            // Create empty runs/ so downstream visualize.py doesn't crash
            std::system("mkdir -p runs");
            shutdownGui();
            return 0;
        }

        if (config.countCategory("seeker") == 0) {
            std::cout << "No seekers placed. Cannot run simulation.\n";
            shutdownGui();
            return 0;
        }
        if (config.countCategory("target") == 0) {
            std::cout << "No targets placed. Cannot run simulation.\n";
            shutdownGui();
            return 0;
        }

        // ── Iteration setup ──────────────────────────────────────────

        int numIterations = guiControl.iterationCount.load();
        double noiseIncrement = guiControl.noiseIncrement.load();
        double startingNoise = config.getMaxNoiseLevel();

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
            stampConfiguredUnits(map, config);

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

        shutdownGui();

    } catch (const std::exception& e) {
        shutdownGui();
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}