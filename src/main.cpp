#include "mapCreation.h"
#include "spawnConfig.h"
#include "mapVisualizer.h"
#include "simulationVisualizer.h"
#include "simulation.h"
#include "simulationRecorder.h"
#include "utils/logger.h"
#include <windows.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <limits>
#include <string_view>

// ????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
//  HELPERS
// ????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????

[[nodiscard]] static constexpr int unitTypeFromString(std::string_view type) noexcept {
    using namespace std::string_view_literals;
    if (type == "seeker"sv)      return MapCreation::SEEKER;
    if (type == "target"sv)      return MapCreation::TARGET;
    if (type == "detector"sv)    return MapCreation::DETECTOR;
    if (type == "interceptor"sv) return MapCreation::INTERCEPTOR;
    if (type == "attacker"sv)    return MapCreation::ATTACKER;
    return MapCreation::WATER;
}

static void stampUnitsOnGrid(MapCreation& map, const SpawnConfig& config) {
    for (const auto& unit : config.getUnits()) {
        int unitType = unitTypeFromString(unit.type);
        if (unitType != MapCreation::WATER) {
            map.placeUnit(unit.row, unit.col, unitType);
        }
    }
}

void printUsage(const char* progName) {
    std::cout << "Usage:\n"
              << "  " << progName << " <shapefile.shp> [cells_n]\n"
              << "  " << progName << " --cache <cache_file.txt>\n"
              << "  " << progName << " --scenario <scenario.json> [--visualize]\n"
              << "\nSpawn Tool Controls:\n"
              << "  Left click   - Place unit on water cell\n"
              << "  Right click  - Remove unit\n"
              << "  S key        - Switch to Seeker mode (attacker, red triangle)\n"
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
              << "  Escape       - Close without saving\n"
              << "  A key        - Switch to Attacker mode (red triangle)\n"
              << "                  1=BlueROV2  2=Riptide  3=BlueBoat\n"
              << "                  4=YUCO  5=NemoSens  6=HUGIN\n"
              << "                  7=TB2  8=QueenHornet  9=Shahed\n"
              << "\nLive Visualization (requires --scenario):\n"
              << "  " << progName << " --scenario scenario.json --visualize\n"
              << "  Controls:\n"
              << "    Space  - Pause/Resume  |  +/-  - Speed\n"
              << "    Enter  - Step (paused)  |  L    - Toggle legend\n"
              << "    Esc    - Skip to end\n"
              << "\nOptional Flags:\n"
              << "  --iterations N     Batch mode: run N iterations\n"
              << "  --noise-step D     Batch mode: add D noise per iteration\n"
              << "  --repeat N         GA mode: run the SAME scenario N times (fixed noise,\n"
              << "                     no noise increment) and write one row per run to\n"
              << "                     runs/ga_batch.csv (for the Python GA fitness estimate)\n"
              << "  --seed S           Fixed RNG seed (0 = auto-random per run)\n"
              << "  --max-steps N      Override max simulation steps (default 2000)\n"
              << "  --no-prompt        Do not wait for Enter on exit (headless/batch)\n";
}

int main(int argc, char* argv[]) {
    uuv::Logger::instance().setLevel(uuv::LogLevel::INFO);
    LOG_INFO("UUV Simulation starting (argc=%d)", argc);

    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

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
        bool visualize = false;
        int batchIterations = 1;
        double batchNoiseStep = 0.0;
        int repeatCount = 0;        // 0 = not GA-repeat mode
        unsigned seedOverride = 0;   // 0 = auto-randomize (default)
        bool noPrompt = false;
        int maxSteps = 2000;
        bool recordRun = false;

        // Parse optional flags (can be at any position)
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--visualize") {
                visualize = true;
            } else if (arg == "--record") {
                recordRun = true;
            } else if (arg == "--iterations" && i + 1 < argc) {
                batchIterations = std::max(1, std::stoi(argv[++i]));
            } else if (arg == "--noise-step" && i + 1 < argc) {
                batchNoiseStep = std::max(0.0, std::stod(argv[++i]));
            } else if (arg == "--repeat" && i + 1 < argc) {
                repeatCount = std::max(1, std::stoi(argv[++i]));
            } else if (arg == "--seed" && i + 1 < argc) {
                seedOverride = std::stoul(argv[++i]);
            } else if (arg == "--no-prompt") {
                noPrompt = true;
            } else if (arg == "--max-steps" && i + 1 < argc) {
                maxSteps = std::max(1, std::stoi(argv[++i]));
            }
        }
        std::cerr << "[DEBUG] recordRun=" << recordRun << " repeatCount=" << repeatCount << std::endl;
        std::cout << "[DEBUG] recordRun=" << recordRun << " repeatCount=" << repeatCount << "\n";

        // ?????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
        //  LOAD MAP
        // ?????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????

        if (firstArg == "--scenario") {
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

            // Reconstruct map from embedded grid data instead of grid_cache.txt
            const MapInfo& info = config.getMapInfo();
            static MapCreation scenarioMap = MapCreation::fromGridData(
                config.getGrid(), info.cellsN, info.canvasWidth, info.canvasHeight);
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

        // ?????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
        //  SPAWN TOOL (if not loading a scenario)
        // ?????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????

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

            stampUnitsOnGrid(map, config);

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
            stampUnitsOnGrid(map, config);
        }

        config.printSummary();
        map.printGrid();

        // ?????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
        //  RUN SIMULATION
        // ?????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????

        if (config.countType("seeker") == 0 && config.hasAttackerZones()) {
            std::cout << "\n=== GA Preparation Scenario ===\n";
            std::cout << "  Targets:         " << config.countType("target") << "\n";
            std::cout << "  Detectors:       " << config.countType("detector") << "\n";
            std::cout << "  Interceptors:    " << config.countType("interceptor") << "\n";
            std::cout << "  Attacker zones:  " << config.getAttackerZones().size() << "\n";
            std::cout << "  Defender zones:  " << config.getDefenderZones().size() << "\n";
            std::cout << "\nScenario saved to scenario.json - no simulation run.\n";
            std::cout << "Hand this scenario to the Python GA to optimise attacker positions.\n";
            std::filesystem::create_directories("runs");
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

        Simulation sim(map, config, maxSteps, seedOverride);

        if (visualize) {
            std::cout << "\nOpening live visualizer...\n";
            std::cout << "Controls: Space=pause  +/-=speed  Enter=step  L=legend  Esc=skip\n\n";

            SimulationVisualizer vis(map, sim);
            SimResult result = vis.run();
            result.print();

            std::filesystem::create_directories("runs");
            result.saveJSON("runs/visualized_result.json");
        }
        else {
            int numIterations = 1;
            double noiseIncrement = 0.0;
            double startingNoise = config.getMaxNoiseLevel();
            bool useBatchFlags = (batchIterations > 1 || batchNoiseStep > 0.0);

            // ?????? GA repeat mode ????????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
            // Runs the SAME scenario N times at FIXED noise, writing one
            // row per run to runs/ga_batch.csv. This is the C++/Python
            // hand-off: the Python GA calls this many times to estimate
            // the fitness of a single defender chromosome (each run is a
            // stochastic sample of P(detected)/P(killed)).
            if (repeatCount > 0) {
                std::string runsDir = "runs";
                std::filesystem::create_directories(runsDir);
                std::string gaCsv = runsDir + "/ga_batch.csv";

                std::cout << "\n-- GA repeat mode: " << repeatCount
                          << " run(s) of the SAME scenario (fixed noise "
                          << startingNoise << ") --\n";
                std::cout << "  Writing fitness samples to " << gaCsv << "\n\n";

                // Write header once (overwrite any previous GA batch).
                {
                    std::ofstream hdr(gaCsv, std::ios::trunc);
                    if (!hdr.is_open()) {
                        throw std::runtime_error("Cannot open " + gaCsv);
                    }
                    hdr << "run_id,probability_detected,probability_killed,"
                           "total_deployment_cost,effectiveness,"
                           "targets_destroyed,total_targets,"
                           "blue_cost,red_cost,loss_exchange_ratio\n";
                    hdr.close();
                }

                for (int r = 0; r < repeatCount; r++) {
                    unsigned iterSeed = (seedOverride != 0) ? seedOverride + static_cast<unsigned>(r) : 0;

                    std::cout << "  Run " << (r + 1) << "/" << repeatCount
                              << "  |  Seed: " << iterSeed << "\n";

                    map.clearAllUnits();
                    stampUnitsOnGrid(map, config);

                    Simulation iterSim(map, config, maxSteps, iterSeed);

                    SimulationRecorder* pRecorder = nullptr;
                    if (recordRun) {
                        SimulationRecorder::RunMetadata meta;
                        meta.runId = r;
                        meta.scenarioName = "ga_repeat_" + std::to_string(r);
                        meta.mapHash = "unknown";
                        meta.maxSteps = maxSteps;
                        meta.noiseLevel = config.getMaxNoiseLevel();
                        meta.seed = iterSeed;
                        meta.startTime = SimulationRecorder::timestampNow();
                        meta.wallTimeMs = 0.0;
                        meta.totalSeekers = 0;
                        meta.totalTargets = 0;
                        meta.totalDetectors = 0;
                        meta.totalInterceptors = 0;
                        meta.totalAttackers = 0;

                        pRecorder = new SimulationRecorder(meta);
                        iterSim.setRecorder(pRecorder);
                    }

                    SimResult result = iterSim.run();
                    result.print();

                    if (recordRun) {
                        std::ostringstream recPath;
                        recPath << runsDir << "/recording_" << r << ".json";
                        bool saved = iterSim.saveRecording(recPath.str());
                        std::cout << "  Recording saved to " << recPath.str() << " (" << (saved ? "OK" : "FAIL") << ")\n";
                        delete pRecorder;
                    }

                    double effectiveness = result.probabilityDetected * result.probabilityKilled;

                    std::ofstream ga(gaCsv, std::ios::app);
                    if (!ga.is_open()) {
                        throw std::runtime_error("Cannot open " + gaCsv);
                    }
                    ga << r << ","
                       << result.probabilityDetected << ","
                       << result.probabilityKilled << ","
                       << result.totalDeploymentCost << ","
                       << effectiveness << ","
                       << result.targetsDestroyed << ","
                       << static_cast<int>(result.targetResults.size()) << ","
                       << result.blueCost << ","
                       << result.redCost << ","
                       << result.lossExchangeRatio << "\n";
                    ga.close();
                }

                std::cout << "\n========================================\n";
                std::cout << "  GA batch complete: " << repeatCount
                          << " samples written to " << gaCsv << "\n";
                std::cout << "========================================\n\n";
            }
            else {
            if (useBatchFlags) { numIterations = batchIterations; noiseIncrement = batchNoiseStep; } else {

            std::cout << "\n-- Iteration Configuration --\n";
            std::cout << "Starting noise level: " << startingNoise << "\n";
            std::cout << "Number of iterations (1 = single run): ";
            std::cin >> numIterations;
            if (numIterations < 1) numIterations = 1;

            if (numIterations > 1) {
                std::cout << "Noise increment per iteration: ";
                std::cin >> noiseIncrement;
                if (noiseIncrement < 0.0) noiseIncrement = 0.0;
            }
            }

            std::string runsDir = "runs";
            std::filesystem::create_directories(runsDir);

            std::cout << "\n-- Running " << numIterations << " iteration(s) --\n";
            if (numIterations > 1) {
                std::cout << "  Noise range: " << startingNoise
                          << " -> " << (startingNoise + noiseIncrement * (numIterations - 1))
                          << " (step " << noiseIncrement << ")\n";
            }
            std::cout << "  Results saved to " << runsDir << "/\n\n";

            for (int iter = 0; iter < numIterations; iter++) {
                double currentNoise = startingNoise + noiseIncrement * iter;
                config.setMaxNoiseLevel(currentNoise);

                // Fixed seed: derive per-iteration seeds so batch runs
                // are deterministic but still distinct. Seed 0 = auto-random.
                unsigned iterSeed = (seedOverride != 0) ? seedOverride + static_cast<unsigned>(iter) : 0;

                std::cout << "\n========================================\n";
                std::cout << "  Iteration " << (iter + 1) << " / " << numIterations
                          << "  |  Noise: " << currentNoise << "\n";
                std::cout << "             Seed: " << iterSeed << "\n";
                std::cout << "========================================\n";

                map.clearAllUnits();
                stampUnitsOnGrid(map, config);

                Simulation iterSim(map, config, maxSteps, iterSeed);
                SimResult result = iterSim.run();
                result.print();

                if (recordRun) {
                    SimulationRecorder::RunMetadata meta;
                    meta.runId = iter;
                    meta.scenarioName = "iteration_" + std::to_string(iter);
                    meta.mapHash = "unknown";
                    meta.maxSteps = maxSteps;
                    meta.noiseLevel = config.getMaxNoiseLevel();
                    meta.seed = iterSeed;
                    meta.startTime = SimulationRecorder::timestampNow();
                    meta.wallTimeMs = 0.0;
                    meta.totalSeekers = static_cast<int>(result.seekerResults.size());
                    meta.totalTargets = static_cast<int>(result.targetResults.size());
                    meta.totalDetectors = static_cast<int>(result.detectorResults.size());
                    meta.totalInterceptors = static_cast<int>(result.interceptorResults.size());
                    meta.totalAttackers = static_cast<int>(result.attackerResults.size());

                    SimulationRecorder recorder(meta);
                    iterSim.setRecorder(&recorder);
                    // Re-run with recorder attached (lightweight: just records state)
                    Simulation recSim(map, config, maxSteps, iterSeed);
                    recSim.setRecorder(&recorder);
                    recSim.run();
                    std::ostringstream recPath;
                    recPath << runsDir << "/recording_" << iter << ".json";
                    recorder.saveJSON(recPath.str());
                    std::cout << "  Recording saved to " << recPath.str() << "\n";
                }

                std::ostringstream filename;
                filename << runsDir << "/run_" << iter << ".json";
                result.saveJSON(filename.str());

                // Append one cost-benefit row to the shared summary CSV
                // (one row per run, header auto-created on first call).
                result.saveCSV(runsDir + "/summary.csv", iter);
            }

            if (numIterations > 1) {
                std::cout << "\n========================================\n";
                std::cout << "  All " << numIterations << " iterations complete.\n";
                std::cout << "  Results saved to " << runsDir << "/\n";
                std::cout << "========================================\n\n";
            }
            }
        }

        if (!noPrompt) {
            std::cout << "\nPress Enter to exit and review logs...";
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cin.get();
        }

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Fatal error: unknown exception" << std::endl;
        return 1;
    }

    return 0;
}


