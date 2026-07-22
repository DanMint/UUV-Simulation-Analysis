# UUV Simulation

A C++ simulation of Unmanned Underwater Vehicles (UUVs) navigating nautical environments. Seekers pathfind toward targets using A* search while detectors attempt to intercept them. Environmental noise simulates real-world conditions like waves and wind.

## Overview

The simulation reads a NOAA nautical shapefile, converts it into a water/land grid, and lets the user place units on the map. Seekers use A* with an Octile heuristic to find optimal paths to targets. Detectors act as stationary interceptors that destroy seekers within a configurable radius. A noise parameter displaces seekers from their planned paths each step, simulating environmental interference.

The system supports batch runs where noise increases with each iteration, producing a set of JSON result files for analysis.

## Project Structure

```
UUV-Simulation-With-Steps/
├── src/
│   ├── main.cpp                    # Entry point, iteration loop, terminal prompts
│   ├── agents/
│   │   └── agent.h                 # SeekerAgent, TargetAgent, DetectorAgent structs
│   ├── mapCreation/
│   │   ├── mapCreation.h/cpp       # Shapefile → grid conversion (GDAL/OGR)
│   │   └── Readme.md
│   ├── mapVisualizer/
│   │   └── mapVisualizer.h/cpp     # SFML spawn tool for placing units
│   ├── pathfinding/
│   │   └── pathfinding.h/cpp       # A* search with Octile heuristic
│   ├── simulation/
│   │   ├── simulation.h/cpp        # Simulation loop, noise, detector checks
│   │   └── simResult.h/cpp         # Results storage, JSON export, console print
│   └── spawnConfig/
│       └── spawnConfig.h/cpp       # Scenario serialization (units, grid, settings)
├── Maps/                           # NOAA shapefiles
├── runs/                           # Output JSON results per iteration
├── tests/
│   └── test_simulation.cpp
├── CMakeLists.txt
├── run.sh                          # Build + run + visualize
└── visulaize.py                    # Python visualization of results
```

## Pipeline

1. **Shapefile Ingestion** — `MapCreation` reads a `.shp` file using GDAL/OGR, scales polygon geometry to a virtual canvas, and rasterizes it into an N×N grid. Each cell is classified as water or land using 9-point sampling with a seam gap cleanup pass.

2. **Spawn Tool** — `MapVisualizer` opens an SFML window where the user places seekers, targets, and detectors on water cells. Detector radius and noise level are adjusted with keyboard controls.

3. **Simulation** — `Simulation` runs a step-based loop. Each step: seekers move one cell along their A* path, noise displaces them, detectors check for interceptions, and target collisions are evaluated.

4. **Results** — `SimResult` stores per-agent outcomes and summary statistics, exports to JSON, and prints to console.

## A* Pathfinding

Seekers find optimal paths using A* search with 8-directional movement on the grid. Cardinal moves (up, down, left, right) cost 1.0 and diagonal moves cost √2 ≈ 1.414.

### Octile Heuristic

The heuristic function estimates the remaining cost from any cell to the destination. For 8-directional movement, the optimal open-field strategy is to move diagonally for `min(dx, dy)` steps, then move straight for the remaining `|dx - dy|` steps. This gives:

```
h = max(dx, dy) + (√2 - 1) × min(dx, dy)
```

This is the tightest admissible heuristic for 8-way grids. It never overestimates the true cost, so A* is guaranteed to find the optimal path. A tighter heuristic means fewer nodes are expanded, making the search faster.

### Grid Cell Values

| Value | Type       | Passable |
|-------|------------|----------|
| 0     | Water      | Yes      |
| 1     | Land       | No       |
| 2     | Seeker     | Yes      |
| 3     | Target     | Yes      |
| 4     | Detector   | Yes      |
| 5     | Interceptor| Yes      |
| 6     | ATK Zone   | No       |
| 7     | Attacker   | Yes      |

## Noise Model

The simulation has a **Max Noise Level** parameter `N` that simulates environmental factors like waves and wind. Noise is not added inside the A* algorithm. A* always computes the optimal path. Noise is applied after A* has decided the next position.

Each simulation step follows this sequence:

1. A* computes the optimal path from the seeker's current position to the target.
2. The seeker moves one cell along the path to position `(x2, y2)`.
3. Two random numbers `rx, ry` are drawn uniformly from `[-N, N]`.
4. The seeker's actual position becomes `(x2 + rx, y2 + ry)`.
5. If the displaced position is out of bounds, on land, or the line between the old and new position crosses land (Bresenham's line check), the displacement is discarded.
6. The pre-computed A* path is invalidated since the seeker is no longer on it.
7. A* recomputes a new path from the displaced position on the next step.

This creates a feedback loop: A* plans the optimal route, noise knocks the seeker off course, A* replans, noise displaces again. Seekers still converge toward the target but follow a noisy, zigzagging trajectory.

## Detectors

Detectors are stationary interceptors placed on the defender side.

- **Persistent** — can intercept unlimited seekers
- **Invisible** — seekers are unaware of detectors and do not path around them
- **Radius-based** — any seeker within Euclidean distance of the detector's radius is destroyed
- **Priority** — detector interception is checked before target collision each step

## Iteration Runs

After placing units in the spawn tool, the terminal prompts for the number of iterations and a noise increment. The simulation runs multiple times with increasing noise, saving each result to `runs/<noise_level>.json`. The grid and unit positions reset between iterations so each run starts from the same initial state.

Example with 5 iterations, starting noise 0.0, increment 0.5:

```
runs/
├── 0.json
├── 0.5.json
├── 1.json
├── 1.5.json
└── 2.json
```

## Spawn Tool Controls

| Key         | Action                              |
|-------------|-------------------------------------|
| Left click  | Place unit on water cell            |
| Right click | Remove unit                         |
| S           | Switch to Seeker mode               |
| T           | Switch to Target mode               |
| D           | Switch to Detector mode             |
| + / -       | Adjust detector radius (±0.5 cells) |
| [ / ]       | Adjust noise level (±0.1)           |
| C           | Clear all units                     |
| Enter       | Save scenario and run               |
| Escape      | Close without saving                |

## Build and Run

### Dependencies

- C++17 compiler
- CMake 3.14+
- SFML 3.x (or 2.x fallback)
- GDAL/OGR
- Python 3 with matplotlib (for visualization)

### Quick Start

```bash
chmod +x run.sh
./run.sh
```

Or manually:

```bash
cmake -S . -B build
cmake --build build -j
./build/uuv_sim Maps/test1/Harbour_Depth_Area.shp 100
./myEnv/bin/python3 visulaize.py
```

### Loading a Saved Scenario

```bash
./build/uuv_sim --scenario scenario.json
```

This skips the spawn tool and runs the simulation directly with the saved unit positions and settings.

## Attacker Agents (Nadeem — Nadeem-Branch)

Adds a multi-platform attacker agent system to the simulation. Press **A** in the spawn tool to enter attacker mode, then **1-9** to select a vehicle type before clicking to place.

Supported platforms:
- **1** BlueROV2 — 1-3 kn, $6k, 300k-450k Hz
- **2** Riptide Micro — 2-5 kn, $15k-45k, 200k-400k Hz
- **3** BlueBoat — 2-6 kn, $5k, 450k-650k Hz (surface)
- **4** YUCO Carrier — 2-6 kn, $50k-100k, 300k-600k Hz
- **5** NemoSens — 2-4 kn, $60k-115k, 200k-500k Hz
- **6** HUGIN Superior — 2-5 kn, $2M-4M, 200k-400k Hz
- **7** Bayraktar TB2 — 90-110 kn, $2M-5M, aerial
- **8** Queen Hornet — 38-43 kn, $1k-5k, aerial
- **9** Shahed 136 — 90-100 kn, $20k-50k, aerial

Each agent runs a full FSM lifecycle (S0-S9) with terminal output showing state transitions, path cost, detection status, and mission outcome. Aerial agents are not detectable by hydrophone arrays.
