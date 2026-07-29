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
```

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
python scripts/visualize.py runs/
```

This prints a table and generates `analysis_plots.png` with 4 charts:
- Steps to completion vs noise
- Seeker success rate vs noise
- Target loss rate vs noise
- Attacker loss rate vs noise

Requires `matplotlib`. Without it, only text output is shown.

## GA Zone System

The spawn tool supports drawing **zones** (rectangular areas):
- **Z key**: Draw ATTACKER zones — the Python GA will place seekers inside these
- **X key**: Draw DEFENDER zones — the GA places detectors/interceptors inside these

Save the scenario with **Enter**, then give `scenario.json` to the Python GA script.

## Build Status

[![Build](https://img.shields.io/badge/build-passing-brightgreen)]()

- `uuv_sim` → ✅ Builds, links, and runs
- `test_simulation` → ✅ Passes
- `test_attacker` → ✅ Passes
- SFML 3 API (all `setPosition(Vector2f)`, `font.openFromFile()`, `event->is<T>()`)

All VSCode IntelliSense errors in `simulationVisualizer.cpp` and `mapVisualizer.cpp` are **false positives** — the IDE cannot resolve `#include <SFML/Graphics.hpp>` because vcpkg include paths aren't configured in VSCode's `c_cpp_properties.json`. The actual build succeeds.
