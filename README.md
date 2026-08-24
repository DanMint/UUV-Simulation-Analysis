# UUV Simulation — Sense-Then-Shoot Naval Engagement Simulator

A grid-based simulation of autonomous underwater/aerial vehicles using a **sense-then-shoot** doctrine. Built with SFML 3, GDAL, and C++20.

## Quick Start

```powershell
# Configure (one-time)
cd windows_build
cmake -B build -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"

# Build
cmake --build build --config Release --target uuv_sim

# Run with a shapefile
cd build/Release
uuv_sim.exe path/to/bathymetry.shp
```

## All Controls (Cheat Sheet)

### Spawn Tool (placing units on the map)

| Key | Action |
|-----|--------|
| **S** | Place **Seeker** (attacker, red circle) |
| **T** | Place **Target** (defender, blue square) |
| **D** | Place **Detector** (sensor, orange circle) |
| **I** | Place **Interceptor** (effector, purple circle) |
| **A** | Place **Attacker** (vehicle, type-coloured) then **1-9** for vehicle |
| **Z** | Draw **ATTACKER spawn zone** (for GA) |
| **X** | Draw **DEFENDER spawn zone** (for GA) |
| **Q** | Toggle **GA-prep mode** (targets + zones only) |
| **Left click** | Place selected unit on a water cell |
| **Right click** | Remove unit from cell |
| **+ / -** | Adjust detector sensing radius |
| **{ / }** | Adjust interceptor kill radius |
| **[ / ]** | Adjust noise level (wave/wind) |
| **C** | Clear all units (preserves zones) |
| **Enter** | Save scenario & run simulation |
| **Escape** | Close without saving |

### Attacker vehicles (press A then a number)

| Key | Vehicle | Type |
|-----|---------|------|
| **1** | BlueROV2 | Underwater ROV |
| **2** | Riptide | Autonomous underwater vehicle |
| **3** | BlueBoat | Unmanned surface vessel |
| **4** | YUCO | Underwater glider |
| **5** | NemoSens | Sensor platform |
| **6** | HUGIN | Military AUV |
| **7** | TB2 | Aerial drone (UAV) |
| **8** | QueenHornet | Aerial drone |
| **9** | Shahed | One-way attack drone |
| **DV** | Anduril Dive-LD | Long-endurance AUV (planning baseline) |

### Live Visualizer (watching the simulation)

| Key | Action |
|-----|--------|
| **Space** | Pause / Resume |
| **+** | Speed up (600ms → 5ms per step) |
| **-** | Slow down (5ms → 2000ms per step) |
| **Enter** | Step once (while paused) |
| **L** | Toggle legend overlay |
| **Escape** | Skip to end & close |

### Command-line flags

```powershell
uuv_sim.exe map.shp              # Load shapefile → spawn tool → simulate
uuv_sim.exe --cache cache.txt    # Load cached grid
uuv_sim.exe --scenario scenario.json              # Load saved scenario
uuv_sim.exe --scenario scenario.json --visualize  # With live visualizer
uuv_sim.exe --cache cache.txt --visualize         # Visualization from cache
uuv_sim.exe --scenario scenario.json --iterations 10 --noise-step 0.1  # Non-interactive batch
uuv_sim.exe --scenario scenario.json --seed 42                        # Deterministic run
uuv_sim.exe --scenario scenario.json --max-steps 5000                 # Extend step budget
uuv_sim.exe --scenario scenario.json --no-prompt                      # No press-Enter on exit
uuv_sim.exe --scenario scenario.json --seed 42 --iterations 5 --no-prompt  # Reproducible batch
```

### Flags

| Flag | Effect |
|------|--------|
| `--visualize` | Open the SFML live visualizer |
| `--iterations N` | Run `N` batch iterations non-interactively |
| `--noise-step S` | Increment noise level by `S` per batch iteration |
| `--seed S` | Fixed RNG seed for reproducible results. `0` (default) = auto-random. In batch mode each iteration uses `seed + iteration`. |
| `--max-steps N` | Override the default 2000-step simulation budget |
| `--no-prompt` | Skip the "Press Enter to exit" prompt (headless automation) |

The `--iterations N --noise-step S` flags run `N` simulations non-interactively
(skipping the prompt), incrementing the noise level by `S` each run. Each run is
saved as JSON to `runs/` and one cost-benefit row is appended to `runs/summary.csv`.
With `--seed S`, batch runs are deterministic and reproducible.

## Architecture

```
src/
├── main.cpp                 — Entry point: loads map, runs spawn tool or sim
├── mapCreation/             — Reads shapefiles (GDAL), classifies water/land
│   ├── mapCreation.h
│   └── mapCreation.cpp
├── agents/                  — Agent types with A* pathfinding
│   ├── seekerAgent.cpp      — Chases targets (A* pathfinding)
│   ├── targetAgent.cpp      — Flees from seekers (evasive)
│   ├── detectorAgent.cpp    — Detects seekers in range (sense!)
│   ├── interceptorAgent.cpp — Kills detected attackers (shoot!)
│   ├── attackerAgent.cpp    — Vehicle with specs (BlueROV2, TB2, etc.)
│   └── vehicleSpecs.cpp     — Vehicle performance profiles
├── simulation/              — Core simulation loop
│   ├── simulation.h         — Simulation class, stepOnce(), run()
│   ├── simulation.cpp       — The "sense-then-shoot" logic
│   ├── simResult.h/cpp      — JSON-serialisable results
│   └── pathfinding/         — A* implementation
├── spawnConfig/             — Scenario I/O (JSON save/load)
├── mapVisualizer/           — SFML spawn tool (place units on map)
├── simulationVisualizer/    — SFML live simulation viewer
└── scripts/
    └── visualize.py         — Python analysis of results (matplotlib)

tests/
├── test_simulation.cpp
└── test_attacker.cpp

windows_build/
└── CMakeLists.txt           — Windows-specific CMake with vcpkg
```

## Agent Types

| Agent | Color | Shape | Role |
|-------|-------|-------|------|
| **Seeker** | Red | Circle | Finds and hits targets using A* pathfinding. Must detect target first (sense-then-shoot). |
| **Target** | Blue | Square | Defensive asset that tries to evade seekers. Moves away when a seeker is detected. |
| **Detector** | Orange | Circle | Senses seekers within its **sensing radius** (adjustable). If a seeker is in range, detectors notify interceptors. |
| **Interceptor** | Purple | Circle | Kills any `attacker` (seeker) within its **kill radius**. Only activates when detector spots someone. |
| **Attacker** | Varies | Circle (larger) | Vehicle with unique specs (speed, endurance, payload). Detected attackers glow with red outline. |

## The Doctrine: Sense-Then-Shoot

This is the core military doctrine the simulation models:

1. **Seekers** roam the map using A* pathfinding toward their assigned target
2. **Detectors** sense a circular area. If a seeker enters this radius → the seeker is **detected**
3. **Interceptors** guard the area. If a seeker is detected AND in kill range → the seeker is **killed**
4. **Seekers** that survive detection AND reach their target → **hit** the target

This forces the simulation to answer: *"How many seekers can get through the defence?"*

## Key Algorithms

### A\* Pathfinding

Every seeker and attacker uses A\* to find the shortest path to its assigned target.
The heuristic is **Octile distance** (tightest admissible for 8-directional grids):

```
h(n) = max(dx, dy) + (√2 - 1) × min(dx, dy)
```

Movement costs:
- Cardinal (up/down/left/right): `1.0`
- Diagonal: `√2 ≈ 1.414`

Paths are recomputed when:
- The target is destroyed (seeker re-targets to nearest living target)
- Environmental noise displaces the agent (path invalidated)

### Noise Model

Wave/wind noise randomly displaces agents each step:
- Magnitude: uniform `[-maxNoise, +maxNoise]` in rows and cols
- **Bresenham line-of-sight enforced**: the agent cannot jump over land
- Path is invalidated on displacement (must recompute via A\*)

### Interceptor Kill Probability Tiers

Distance-tiered probabilistic engagement model:

| Zone | Distance ratio | Kill chance |
|------|---------------|-------------|
| Inner | 0% – 50% of radius | **90%** |
| Mid | 50% – 70% of radius | **60%** |
| Outer | 70% – 100% of radius | **50%** |
| Beyond radius | > 100% | **0%** |

This models real weapon systems: closer = deadlier.

## Batch Mode (Multiple Noise Levels)

When running without `--visualize`, you get an interactive prompt:

```
Starting noise level: 0.5
Number of iterations (1 = single run): 5
Noise increment per iteration: 0.2

-- Running 5 iterations --
  Noise range: 0.5 -> 1.3 (step 0.2)
  Results saved to runs/
```

Each iteration runs independently with increasing noise. Results saved as JSON to `runs/`.

## Analysing Results

```bash
cd UUV-Simulation-Analysis
python scripts/visualize.py runs/          # Plots seeker/attacker paths per run
python scripts/analyze_costs.py runs/      # Cost-benefit charts from summary.csv
```

`scripts/visualize.py` plots one figure per run JSON: water/land grid, seeker
paths (red), attacker paths (dashed, colour-coded by vehicle type), targets,
detectors and interceptors. Outputs PNGs to `paths/`.

`scripts/analyze_costs.py` reads `runs/summary.csv` (one row per batch run, written
by `SimResult::saveCSV`) and prints a cost-benefit summary plus three plots.

### Cost-Benefit Definitions (Lance's Formula)

The simulation tracks two cost metrics for the loss-exchange ratio:

| Cost | Definition | What it measures |
|------|-----------|------------------|
| **Blue cost** | `Σ(interceptor.engagementCount × interceptor.engagementCost)` | **Every shot fired by interceptors** — hits AND misses. Engagement cost defaults to `$50,000/shot` (configurable via `DEFAULT_COST_PER_SHOT`). This captures the "$2M interceptor vs $1000 drone" trade: a single interceptor shot costs more than a cheap drone. |
| **Red cost** | `Σ(attacker.unitCostMin)` | **Total cost of ALL attackers deployed** — sunk cost the moment they are committed, regardless of mission success. |
| **Loss exchange ratio** | `red_cost / blue_cost` | `<1` = defence efficient (blue spends less than red's deployed assets), `>1` = attackers trade up (red's cheap swarm forces expensive blue intercepts). |

**Example**: 10 Shahed drones (`$20k` each = `$200k` red cost) vs 4 interceptor shots
(`$50k` each = `$200k` blue cost) → loss exchange ratio = `1.0` (breakeven).
If 3 interceptors fire 12 shots (`$600k` blue) to stop 10 Shaheds (`$200k` red) →
ratio = `0.33` → inefficient defence (blue spending 3× red's deployed cost).

### CSV columns

```
run_id,blue_cost,red_cost,loss_exchange_ratio,targets_destroyed,total_targets,
critical_asset_reached,total_steps,mission_success_rate,interceptor_engagements
```

### Plots

- Cost trade-off (blue vs red, colour = success rate) → `runs/cost_tradeoff.png`
- Mission success vs sim length → `runs/success_vs_steps.png`
- Loss-exchange-ratio histogram → `runs/loss_exchange_ratio.png`

Requires `numpy` and `matplotlib`.

## GA Zone System

The spawn tool supports drawing **zones** (rectangular areas):
- **Z key**: Draw ATTACKER zones — the Python GA will place seekers inside these
- **X key**: Draw DEFENDER zones — the GA places detectors/interceptors inside these

Save the scenario with **Enter**, then give `scenario.json` to the Python GA script.

## Anduril Dive-LD Baseline Scenario

**Phase 17 Integration**: The planning team specifies *"3 Anduril Dive-LD AUVs come from three different directions."* This is captured as a reusable, headless baseline fixture for GA fitness evaluation.

### Vehicle Registry Entry

`diveld` is registered in `vehicleSpecs.cpp` as a Anduril long-endurance vehicle:

| Parameter | Value |
|-----------|-------|
| Manufacturer | Anduril Industries |
| Type | Long-endurance AUV (hydrophone-detectable UUV) |
| Speed | 1–3 kn (conservative estimate; `stepDelay = 4` in sim) |
| Acoustic emission | 200–400 kHz |
| Unit cost | `$500k – $1M` (⚠ **planning estimate**, no public list price) |
| Cost category | `premium` |
| Short code | `DV` |
| Water capability | Shallow-water capable, NOT aerial, NOT surface |
| Endurance | 72+ hours (long-endurance class) |

> ⚠ Cost/speed figures are **engineering estimates** for planning comparison. Flagged with `SPEC ESTIMATE` in `vehicleSpecs.cpp`.

### Reference Scenario: `scenarios/diveld_baseline_complete.json`

**Purpose**: Provides a canonical "three directions" baseline for GA hyperparameter tuning and loss-exchange-ratio analysis.

**Scenario composition**:
- **3× Dive-LD attackers** spawned at three distinct cells from three directions:
  - **North**: Row 10, Col 10 (upper-left approach)
  - **South**: Row 65, Col 45 (lower-center approach)
  - **East**: Row 40, Col 85 (right-flank approach)
- **1× Critical target** (Shahed-class): Row 35, Col 45 (harbor center)
- **Defenders** (Pearl Harbour scenario, 2x detectors + 1x interceptor):
  - Detector 1: Row 30, Col 45 (north-facing)
  - Detector 2: Row 40, Col 40 (west-facing)
  - Interceptor 1: Row 35, Col 50 (central position)
- **Map**: Pearl Harbour shallow-water zone (100×100 grid, mixed land/water)
- **Detector radius**: 4 cells (≈ 500 m in harbor scale)
- **Interceptor radius**: 3 cells (≈ 375 m effective kill zone)

**Run the baseline** (headless, reproducible):

```powershell
# Run a single iteration with fixed seed (reproducible)
.\windows_build\build\Release\uuv_sim.exe --scenario scenarios/diveld_baseline_complete.json `
  --iterations 1 --seed 42 --no-prompt

# Run GA parameterization series (5 iterations, increasing noise)
.\windows_build\build\Release\uuv_sim.exe --scenario scenarios/diveld_baseline_complete.json `
  --iterations 5 --noise-step 0.1 --seed 42 --no-prompt

# With live visualizer (headful mode)
.\windows_build\build\Release\uuv_sim.exe --scenario scenarios/diveld_baseline_complete.json --visualize
```

**Unit test**:

```powershell
# Test that Dive-LD specs are valid and scenario loads correctly
.\windows_build\build\Release\test_diveld_scenario.exe

# Expected output:
#   ✓ Dive-LD vehicle specs valid
#   ✓ 3 Dive-LD attackers confirmed
#   ✓ Critical target confirmed
#   ✓ Defenders configured (2 detectors, 1 interceptor)
#   ✓ ALL DIVE-LD BASELINE VALIDATIONS PASSED
```

### Legacy: `scenarios/diveld_baseline.json`

Generated by `scripts/make_diveld_scenario.py` (original simpler baseline):

```powershell
# Regenerate (deterministic, seed 42)
python scripts/make_diveld_scenario.py --out scenarios/diveld_baseline.json --seed-grid scenario.json --seed 42

# Run
.\windows_build\build\Release\uuv_sim.exe --scenario scenarios/diveld_baseline.json --iterations 1 --seed 42 --no-prompt
```

## New Features

### Simulation Recording & Replay

The simulator can now record complete simulation state at each step for later replay and analysis:

```powershell
# Run with recording enabled
.\windows_build\build\Release\uuv_sim.exe --scenario scenarios/diveld_baseline_complete.json --record

# Outputs:
#   runs/recording_0.json  - Full step-by-step state
#   runs/run_0.json        - Final results
#   runs/summary.csv       - Aggregated metrics
```

Recording JSON includes:
- Agent positions, alive status, and detection state per step
- Event stream (detections, interceptions, target destructions)
- Per-agent statistics (path length, time alive, alive at end)

### Web Dashboard

A FastAPI-based dashboard provides real-time visualization and analysis:

```powershell
# Start the dashboard API
python scripts/dashboard_api.py

# Open in browser
http://localhost:8000
```

Dashboard features:
- Run selection and summary statistics
- Agent position charts over time
- Simulation replay controls (play/pause, step forward/back, speed control)
- Agent detail panel (click any agent in the chart to see trajectory)
- Real-time event log
- Export buttons (PNG, CSV, JSON)

### Configurable Heartbeat Logging

Simulations can now log heartbeat messages at configurable intervals:

```powershell
# Log heartbeat every 100 steps
.\windows_build\build\Release\uuv_sim.exe --scenario scenarios/diveld_baseline_complete.json --heartbeat-interval 100
```

### Event Filtering

The recorder supports event type filtering for targeted analysis:

```cpp
// Only record detection and intercept events
recorder.setEventFilter(SimulationRecorder::EVENT_DETECTION | SimulationRecorder::EVENT_INTERCEPT);
```

### Python Scripts

- `analyze_ga.py` — Analyze GA convergence, Pareto fronts, and best scenarios
- `analyze_costs.py` — Cost-benefit analysis of simulation batches
- `analyze_sensitivity.py` — Parameter sensitivity analysis (vary sensing radius, kill radius, noise, unit counts)
- `visualize.py` — Plot simulation runs and recordings
- `dashboard_api.py` — Web dashboard with replay and export

## Build Status

[![Build](https://img.shields.io/badge/build-passing-brightgreen)]()

- `uuv_sim` → ✅ Builds, links, and runs
- `test_attackerAgent` → ✅ Passes
- `test_simulation` → ✅ Passes
- `test_diveld_scenario` → ✅ Passes (Dive-LD integration)
- `test_logger` → ✅ Passes
- `test_spatialGrid` → ✅ Passes
- `test_mapCreation` → ✅ Passes
- `test_spawnConfig` → ✅ Passes
- `test_pathfinding` → ✅ Passes
- `test_simulationRecorder` → ✅ Passes
- `test_stress` → ✅ Passes (200×200 map, 45 agents, 1000 steps)
- SFML 3 API (all `setPosition(Vector2f)`, `font.openFromFile()`, `event->is<T>()`)

All VSCode IntelliSense errors in `simulationVisualizer.cpp` and `mapVisualizer.cpp` are **false positives** — the IDE cannot resolve `#include <SFML/Graphics.hpp>` because vcpkg include paths aren't configured in VSCode's `c_cpp_properties.json`. The actual build succeeds.

## Troubleshooting

| Problem | Cause | Fix |
|---------|-------|-----|
| "cannot open source file SFML/Graphics.hpp" | VSCode IntelliSense doesn't know vcpkg paths | Ignore — build succeeds |
| Map is all land | Shapefile not found or empty | Check path to `.shp` file |
| No units on map | Spawn tool closed without placing anything | Place at least 1 seeker + 1 target |
| Simulation finishes immediately | No seekers or no targets placed | Add seekers and targets |
| "No valid A\* path" error | Unit placed on land cell or on another unit | Place on water cells only |
| Window title says nothing | Font file not found | Console shows which fonts were tried |
| Visualizer is black screen | SFML driver issue | Update GPU drivers |
| "Unknown vehicle type" | Typo in scenario JSON or command | Use: `bluerov2`, `riptide`, `blueboat`, `yuco`, `nemosens`, `hugin`, `tb2`, `queenhornet`, `shahed`, `diveld` |
