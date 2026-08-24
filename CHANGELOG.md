# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Genetic Algorithm (GA) infrastructure with Python wrapper
  - Defender optimizer: maximize P(detected) × P(killed) - cost penalty
  - Attacker optimizer: maximize target destruction rate - cost penalty
  - Island model, parallel evaluation, checkpoint/resume, early stopping
- `scripts/genetic_algorithm.py` - Full GA implementation
- `scripts/analyze_ga.py` - GA convergence and Pareto analysis
- `scripts/analyze_ga_pareto.py` - Multi-objective Pareto frontier plots
- `scripts/benchmark_ga.py` - Automated GA benchmark harness
- `scripts/sensitivity_analysis.py` - Parameter sensitivity tool
- `scripts/test_ga_integration.py` - End-to-end GA integration tests
- `scripts/compare_scenarios.py` - Scenario regression testing
- `scripts/ga_batch_cpp.py` - Direct C++ batch integration module
- `docs/ga_usage_guide.md` - Comprehensive GA documentation
- `src/utils/logger.h` - Lightweight structured logging for C++
- `.github/workflows/ci.yml` - GitHub Actions CI pipeline
- `tests/test_logger.cpp` - Logger unit tests
- `tests/test_diveld_scenario.cpp` - Dive-LD scenario end-to-end test
- `Simulation::runBatch()` static method for direct C++ batch simulation
- GA batch mode in C++ simulator (`--repeat N --seed S --no-prompt`)
- `probability_detected`, `probability_killed`, `total_deployment_cost` fields in SimResult
- Attacker intercept recording (`attacker.intercepts` vector)
- Detector sighting recording on attackers (not just detectors)
- `visualize.py` GA-specific plotting functions

### Fixed
- Critical C++ bug: interceptors never killed attackers in CSV output
  - Added `attacker.intercepts.push_back()` on successful intercept
  - Added `attacker.recordSighting(detector.id, currentStep)` in detector tracking
- Attacker GA removed all seekers from base scenario, causing simulation to skip
- Defender GA had no attackers to evaluate (added baseline threat package)
- Python GA subprocess failed to find `ga_batch.csv` on Windows (DEVNULL issue)
- Attacker budget scale mismatch (auto-detect: $2M for attackers, 100 for defenders)
- Seeker spawned on land cell in `diveld_baseline_complete.json`
- `windows_build/CMakeLists.txt` stale GDAL configuration
- CMake test targets missing proper GDAL linking
- `simulationRecorder.h` event type bitmask constants (1,2,3,4) caused incorrect filtering
- `dashboard_api.py` deprecated `on_event("shutdown")` replaced with `lifespan` handler
- `analyze_ga.py` float conversion bug for generation field
- `multi_objective_ga.py` self-domination bug in `dominates()`
- `ga_constraints.py` divide-by-zero in ResourceConstraint
- `benchmark_ga.py` working directory and script path resolution
- `main.cpp` duplicate argc check and stray debug output
- CTest working directory issues for tests using relative paths

### Changed
- C++ cost model: detectors/interceptors use `unitCost=1.0` (consistent with Python GA)
- Attacker costs use real vehicle prices from `vehicleSpecs.cpp`
- Python GA fitness functions use numpy for numerical stability
- `analyze_costs.py` updated to handle GA batch CSV format
- `visualize.py` added `diveld` color mapping
- `simulationVisualizer.cpp` refactored trail drawing to eliminate duplication
- `mapVisualizer.cpp` refactored radius drawing to eliminate duplication
- Added 4 new test files: test_mapCreation, test_spawnConfig, test_pathfinding, test_simulationRecorder
- Added test_stress.cpp for large-scale simulation stability testing
- All tests run from repo root via CTest WORKING_DIRECTORY property

### Added
- `scripts/analyze_sensitivity.py` - Parameter sensitivity analysis tool
- `scripts/analyze_costs.py` vehicle-type cost breakdown plots
- `docs/developer_guide.md` recent features section
- `CONTRIBUTING.md` with build/test/PR guidelines

## [0.1.0] - 2025-01-15

### Added
- Initial project structure
- C++ simulation engine with SFML visualization
- Basic scenario spawning tool
- Dive-LD vehicle type support
- Cost-benefit analysis scripts
