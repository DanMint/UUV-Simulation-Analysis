# UUV Simulation — Complete User Guide & Code Reference

## 1. What Is This Project?

This is an **autonomous naval engagement simulator**. You load a real-world undersea map (from a shapefile), place different types of military drones on it, and watch them fight using a **sense-then-shoot** doctrine.

The map is divided into a grid of cells — each cell is either **water** (blue) or **land** (green). Agents move cell-by-cell using **A\* pathfinding** to navigate around obstacles.

---

## 2. Project Architecture

```
UUV-Simulation-Analysis/
├── src/
│   ├── main.cpp                    ← ENTRY POINT
│   │   Loads the map, opens the spawn tool or runs simulation
│   │
│   ├── mapCreation/                ← MAP LOADING (GDAL)
│   │   Reads shapefiles (.shp) containing depth contours
│   │   Classifies each grid cell as WATER or LAND
│   │   Removes "seam gaps" (artifacts between depth contour polygons)
│   │
│   ├── agents/                     ← AGENT TYPES
│   │   ├── seekerAgent.cpp/h      — Red circle, hunts targets with A*
│   │   ├── targetAgent.cpp/h      — Blue square, tries to evade seekers
│   │   ├── detectorAgent.cpp/h    — Orange circle, SENSE only (sees seekers)
│   │   ├── interceptorAgent.cpp/h — Purple circle, SHOOT only (kills seekers)
│   │   ├── attackerAgent.cpp/h    — Vehicle with FSM (12 states, full lifecycle)
│   │   └── vehicleSpecs.cpp/h     — Registry of 9 real-world vehicle types
│   │
│   ├── simulation/                 ← CORE LOOP
│   │   ├── simulation.h/cpp       — Runs steps, checks wins/losses
│   │   ├── simResult.h/cpp        — Stores results, saves to JSON
│   │   └── pathfinding.h/cpp      — A* pathfinding engine
│   │
│   ├── spawnConfig/                ← SCENARIO SAVE/LOAD
│   │   Saves and loads JSON files with map data + unit placements + zones
│   │
│   ├── mapVisualizer/              ← SPAWN TOOL (SFML window)
│   │   Lets you click on the map to place units, draw zones
│   │
│   └── simulationVisualizer/       ← LIVE VISUALIZER (SFML window)
│       Shows the simulation running with trails, HUD, legend
│
├── scripts/
│   └── visualize.py                ← Python analysis (table + plots)
│
├── tests/
│   ├── test_simulation.cpp         ← Compares results against stored baseline
│   └── test_attacker.cpp           ← Tests attacker FSM
│
└── windows_build/
    └── CMakeLists.txt              ← Build config with SFML + GDAL via vcpkg
```

---

## 3. Code Architecture Deep Dive

### 3.1 `main.cpp` — The Entry Point

This is where execution starts. It does 3 things:

**Step 1: Load the map** in one of three ways:
- `--scenario scenario.json` → loads a saved scenario (most common for you)
- `--cache grid_cache.txt` → loads a pre-processed grid
- `path/to/map.shp` → reads raw shapefile, processes into grid

**Step 2: Spawn units** — opens the SFML spawn tool window where you click to place agents on the map (unless loading a scenario that already has units)

**Step 3: Run simulation** in one of two modes:
- `--visualize` → opens the live visualizer window
- (no flag) → headless batch mode, prompts for iterations + noise

### 3.2 `mapCreation.cpp/h` — Map Creation

Reads a **shapefile** (`.shp`) containing depth contours. The key algorithm:

```
1. Load polygons from shapefile (each polygon = a depth contour)
2. For every grid cell, sample 9 interior points and count how many
   are inside water polygons. If ≥5 → cell is WATER. Else → LAND.
3. Run "seam gap cleanup" — some cells get misclassified as land when
   they're really water pockets surrounded by water.
```

Key methods you'll use:
- `getGrid()` — returns the 2D grid (0=water, 1=land, 5+=unit types)
- `isWater(row, col)` — check if a cell is passable water
- `getAllWaterCells()` — list all water cells (for placing units)
- `placeUnit(row, col, type)` — mark a cell with a unit type

### 3.3 `vehicleSpecs.h/cpp` — Vehicle Registry

This is the **single source of truth** for all 9 vehicle types. Each has:

| Field | Example (BlueROV2) | Example (TB2) |
|-------|-------------------|----------------|
| speedKnotsMin/Max | 1-3 kn | 90-110 kn |
| emissionFreqLow/HighHz | 300k-450k Hz | 0-0 Hz (aerial) |
| isAerial | false | true |
| isSurfaceVessel | false | false |
| unitCostMin/Max | $6k-$6k | $2M-$5M |
| stepDelay | 4 (slowest) | 1 (fastest) |
| shallowWaterCapable | true | false |

**Key insight:** `stepDelay` controls movement speed. An agent with `stepDelay=4` moves once every 4 simulation ticks. `stepDelay=1` moves every tick.

### 3.4 `attackerAgent.h/cpp` — The FSM (12-State Machine)

This is your most complex agent. It has a **Finite State Machine** that models a complete military mission:

```
S0_IDLE → S1_RECEIVE_MISSION → S2_VALIDATE → S3_INIT_BEHAVIOR
    → S4_EXECUTE (loops until target reached or killed)
    → S5_LOG_RESULT → S6_UPDATE_SHARED → S7_DEACTIVATE
    → S8_COMPLETE → S9_RESET (ready for next scenario)
```

On error: `FALLBACK → ABORT`

**States explained:**
- **S0_IDLE**: Agent is spawned but waiting. Does nothing.
- **S1_RECEIVE_MISSION**: Target coordinates assigned.
- **S2_VALIDATE**: Checks if target is valid (not same as spawn, reachable).
- **S3_INIT_BEHAVIOR**: Computes A\* path, logs vehicle specs.
- **S4_EXECUTE**: Moves along path. Reports milestones at 25%, 50%, 75%.
- **S5_LOG_RESULT**: Logs outcome (success/failure, steps, cost).
- **S6_UPDATE_SHARED**: Pushes result to GA optimizer (placeholder for Python).
- **S7_DEACTIVATE**: Marks agent inactive (still visible on map).
- **S8_COMPLETE**: Terminal success state.
- **S9_RESET**: Resets for next GA scenario run.

### 3.5 `simulation.cpp/h` — The Core Loop

This is where the "sense-then-shoot" doctrine runs. Each step:

```
1. Move each living seeker one cell along its A* path
2. Update attacker states (their FSM)
3. Apply environmental noise (wave/wind) — displaces positions
4. Detectors scan: check if any seeker is in their sensing radius
   → if so, seeker.detected = true (sticky — lasts forever)
5. Interceptors scan: check if any DETECTED seeker is in kill radius
   → if so, roll probability dice (90%/60%/50% close/mid/far)
6. Check collisions: did a seeker reach a target cell?
   → if so, target is destroyed
7. Check if simulation should end:
   - All targets destroyed? → FINISHED
   - All seekers dead/reached and attackers idle? → FINISHED
   - Hit max steps? → FINISHED
```

**Key methods:**
- `run()` — Full headless run from clean state
- `runFromCurrentState()` — Continue from where you are (no reset)
- `stepOnce()` — Advance exactly one step (used by visualizer)
- `finishFromCurrentState()` — Step until finished (used by Esc key)

### 3.6 `simulationVisualizer.cpp` — The Live Viewer

Opens an SFML window showing the simulation happening in real-time.

**Rendering order** (back to front):
1. **Grid** — water cells (dark blue), land cells (green)
2. **Detector radii** — semi-transparent orange circles
3. **Interceptor radii** — semi-transparent purple circles
4. **Trails** — fading lines showing where agents have been
5. **Agents** — red circles (seekers), blue squares (targets), etc.
6. **Legend** — overlay (toggle with L key)
7. **UI Panel** — black bar at bottom with step count + status

**Key functions:**
- `run()` — The main loop; returns a `SimResult` when done
- `drawGrid()` — Renders the water/land grid
- `drawAgents()` — Draws all agent types with their colors and IDs
- `drawUI()` — The HUD panel with step count, alive/dead stats, status
- `drawLegend()` — Colour key overlay (press L)
- `drawTrails()` — Fading position history lines

---

## 4. Vehicle Types Reference

| # | Type | Manufacturer | Speed (kn) | Cost | Detectable? | stepDelay |
|---|------|-------------|-----------|------|-------------|-----------|
| 1 | BlueROV2 | Blue Robotics | 1-3 | ~$6k | Yes (UUV) | 4 |
| 2 | Riptide | Oceanscience | 2-5 | $15-45k | Yes (UUV) | 3 |
| 3 | BlueBoat | Blue Robotics | 2-6 | ~$5k | No (USV) | 2 |
| 4 | YUCO | Seaber | 2-6 | $50-100k | Yes (UUV) | 2 |
| 5 | NemoSens | RTSYS | 2-4 | $60-115k | Yes (UUV) | 3 |
| 6 | HUGIN | Kongsberg | 2-5 | $2-4M | Yes (UUV) | 3 |
| **DV** | **Dive-LD** | **Anduril** | **1-3** | **$500k-$1M** | **Yes (UUV)** | **4** |
| 7 | TB2 | Bayraktar | 90-110 | $2-5M | No (UAV) | 1 |
| 8 | QueenHornet | Zala Aero | 38-43 | $1-5k | No (UAV) | 1 |
| 9 | Shahed | Iran | 90-100 | $20-50k | No (UAV) | 1 |

**Why this matters:** Aerial drones (TB2, QueenHornet, Shahed) are NOT detectable by hydrophones. They fly over the water — underwater detectors can't see them. This is realistic. **Dive-LD** is a premium long-endurance AUV — detectible by hydrophones, slow but capable in shallow water (e.g., Pearl Harbour).

---

## 5. Complete Controls Reference

### Spawn Tool (Setting up scenarios)

| Key | What it does |
|-----|-------------|
| **S** | Switch to **Seeker** mode — left-click to place red seekers |
| **T** | Switch to **Target** mode — left-click to place blue targets |
| **D** | Switch to **Detector** mode — left-click to place orange detectors |
| **I** | Switch to **Interceptor** mode — left-click to place purple interceptors |
| **A** | Switch to **Attacker** mode — then press **1-9** to pick vehicle type |
| **Left click** | Place current unit type on water cell |
| **Right click** | Remove unit from cell |
| **Z key** | Draw **Attacker spawn zone** — click+drag rectangle, then press Enter |
| **X key** | Draw **Defender spawn zone** — click+drag rectangle, then press Enter |
| **Q key** | Toggle **GA-prep mode** — shows targets + zones only (for transfer to GA) |
| **+ / -** | Increase / decrease detector sensing radius |
| **{ / }** | Increase / decrease interceptor kill radius |
| **[ / ]** | Increase / decrease environmental noise level |
| **C key** | Clear all units (keeps zones!) |
| **Enter** | Save `scenario.json` + run simulation |
| **Escape** | Close without saving anything |
| **1-9** | (while in Attacker mode) Pick vehicle: 1=BlueROV2, 2=Riptide, ..., 9=Shahed |

### Live Visualizer (Watching the simulation)

| Key | What it does |
|-----|-------------|
| **Space** | **Pause** / Resume — freeze the simulation at current step |
| **+** (or =) | **Speed up** — decreases delay, agents move faster |
| **-** (or _) | **Slow down** — increases delay, agents crawl |
| **Enter** | **Step once** — advance exactly 1 step while paused |
| **L** | Toggle **Legend** overlay — shows colour key |
| **Escape** | **Skip to end** & close window |

**Speed ranges:** 5ms (fastest — blur) to 2000ms (slowest — crawl). Default: 600ms.

### Command Line

```powershell
# Load shapefile → open spawn tool → run
uuv_sim.exe bathymetry.shp

# Load shapefile with custom grid resolution (default 100)
uuv_sim.exe bathymetry.shp 200

# Load cached grid (skip shapefile processing)
uuv_sim.exe --cache grid_cache.txt

# Load saved scenario (map + units already placed)
uuv_sim.exe --scenario scenario.json

# Load scenario AND open live visualizer
uuv_sim.exe --scenario scenario.json --visualize

# Cache + visualize
uuv_sim.exe --cache grid_cache.txt --visualize
```

---

## 6. Key Algorithms Explained

### A\* Pathfinding
Every seeker uses A\* to find the shortest path to its target. The path is recomputed when:
- The target is destroyed (seeker re-targets to nearest living target)
- Noise displaces the seeker (path invalidated)

### Sense-Then-Shoot Doctrine
This is the core military model:

```
1. DETECT:   Detector enters seeker in its sensing radius
             → seeker.detected = true (STICKY — can't be undone)
2. TRACK:    Detector logs sighting (seeker ID, step number)
3. ENGAGE:   Interceptor checks if detected seeker is in kill radius
             → rolls dice for kill probability
4. KILL:     If probability roll succeeds → seeker.alive = false
5. EVALUATE: Did all seekers die? Did targets survive? → FINISHED
```

**Critical:** Detectors only SENSE. Interceptors only SHOOT. You need BOTH for the doctrine to work. If you only place detectors, seekers will be spotted but never killed. If you only place interceptors, they'll never fire because nobody is tracking.

### Noise Model
Environmental noise (waves, wind) randomly displaces agents:
- Magnitude: uniform random between `[-maxNoise, +maxNoise]` in rows and cols
- Noise must not move agent onto land (Bresenham line-of-sight checked)
- Agent's path is invalidated on displacement (must recompute)

### Interceptor Kill Probability Tiers
Kill chance depends on distance to target:
- **Inner zone** (0-50% of radius): **90% kill chance**
- **Mid zone** (50-70% of radius): **60% kill chance**
- **Outer zone** (70-100% of radius): **50% kill chance**

This models real weapon systems — closer = deadlier.

---

## 7. Friday Presentation Demo Script

### 15-Minute Walkthrough

**Setup (before):**
```powershell
cd windows_build
cmake -B build -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release --target uuv_sim
copy ..\tests\fixtures\scenario.json build\Release\
```

### Minute 0-2: "What is this?"
Say: *"This is a simulation of autonomous underwater and aerial drones using a military 'sense-then-shoot' doctrine. We load real undersea maps from government bathymetry data, place different types of drones, and watch them fight."*

Point at the README diagram.

### Minute 2-6: "The Spawn Tool" (SHOW THIS)
```powershell
cd build\Release
uuv_sim.exe ..\..\..\data\bathymetry.shp
```
1. The SFML spawn tool opens with the map
2. Press **S** — left-click to place 2-3 red seekers
3. Press **T** — left-click to place 1 blue target far away
4. Press **D** — left-click to place 1 orange detector near the target
5. Press **I** — left-click to place 1 purple interceptor near the target
6. Press **+** a few times — show the detector radius growing
7. Press **Enter** → simulation starts headless

Say: *"I placed seekers (red) that will pathfind to the target (blue) using A\*. The detector (orange) will spot them, and the interceptor (purple) will try to kill them."*

### Minute 6-9: "Live Visualizer" (SHOW THIS)
```powershell
uuv_sim.exe --scenario scenario.json --visualize
```
1. Window opens with agents crawling very slowly (600ms default)
2. Press **Space** → **"Now it's paused. See the HUD at the bottom — S:3/3, T:1/1, A:0/0"**
3. Press **L** → **"This is the legend. Every colour tells you what you're looking at."**
4. Press **Space** again → watch seekers move toward target
5. When a seeker enters the orange circle (detector radius), it turns pink — **"It's been detected!"**
6. If it enters the purple circle (interceptor radius), it may die — **"There goes one. Killed in action."**
7. Press **+** to speed up and watch the rest
8. Press **Escape** to skip to end

### Minute 9-11: "Vehicle Types"
Say: *"We have 9 different vehicle types, each with real-world specs. Let me run a demo."*

Show the vehicle table from README. Highlight:
- **BlueROV2** ($6k, 1-3 kn, slow) vs **TB2** ($2-5M, 90-110 kn, fast)
- **Shahed** ($20-50k, one-way attack drone — cheap and disposable)
- Aerial drones are **immune to hydrophone detection** — they fly over water

### Minute 11-13: "Batch Analysis"
Say: *"We can also run hundreds of simulations with different noise levels to see how weather affects success rates."*

```powershell
# Headless batch run
uuv_sim.exe --scenario scenario.json
# Type: 5 iterations, noise increment 0.2
# Then:
python ../../scripts/visualize.py runs/
```

Show the table output and the `analysis_plots.png` charts.

### Minute 13-15: "Architecture & Q&A"
Say: *"The code is modular: map loading, agents, simulation loop, spawn tool, and visualizer are all separate. The FSM in AttackerAgent has 12 states modelling a full military mission from spawn to completion. Everything is JSON-serialisable for integration with a Python genetic algorithm."*

Take questions.

---

## 8. Common Issues & Troubleshooting

| Problem | Cause | Fix |
|---------|-------|-----|
| "cannot open source file SFML/Graphics.hpp" | VSCode IntelliSense doesn't know vcpkg paths | Ignore — build succeeds |
| Map is all land | Shapefile not found or empty | Check path to .shp file |
| No units on map | Spawn tool closed without placing anything | Place at least 1 seeker + 1 target |
| Simulation finishes immediately | No detectors made it boring | Add detectors + interceptors for drama |
| "No valid A* path" error | Unit placed on land cell or on another unit | Place on water cells only |
| Window title says nothing | Font file not found | Console shows which fonts tried |
| Visualizer is black screen | SFML driver issue | Update GPU drivers |
| "Unknown vehicle type" | Typo in scenario JSON or command | Use: bluerov2, riptide, blueboat, yuco, nemosens, hugin, tb2, queenhornet, shahed |

---

## 9. File-by-File Quick Reference

### Core Files (read these first)

| File | Lines | What it does |
|------|-------|-------------|
| `main.cpp` | ~300 | Entry point — loads map, runs spawn tool or simulation |
| `simulation.cpp` | ~400 | Core loop, termination checks, noise, sense-then-shoot |
| `simulation.h` | ~100 | Simulation class declaration |
| `attackerAgent.cpp` | ~500 | 12-state FSM — the most complex agent |
| `attackerAgent.h` | ~200 | FSM states, factory, detection/interception helpers |
| `vehicleSpecs.cpp` | ~100 | Registry of 9 vehicle types |
| `vehicleSpecs.h` | ~100 | VehicleSpecs struct with computed properties |
| `simulationVisualizer.cpp` | ~500 | SFML rendering, trails, HUD, legend |

### Support Files

| File | Lines | What it does |
|------|-------|-------------|
| `mapCreation.cpp` | ~300 | Shapefile loading, grid classification, seam cleanup |
| `spawnConfig.cpp` | ~300 | Scenario JSON save/load |
| `simResult.cpp` | ~200 | Result formatting, JSON output |
| `seekerAgent.cpp` | ~80 | A* path computing and step movement |
| `targetAgent.cpp` | ~50 | Evasive movement (flees from seekers) |
| `detectorAgent.h` | ~40 | Stationary sensor struct |
| `interceptorAgent.h` | ~30 | Stationary effector struct |
| `mapVisualizer.cpp` | ~400 | SFML spawn tool UI |
| `scripts/visualize.py` | ~200 | Python analysis + plots |
| `tests/test_simulation.cpp` | ~200 | Regression test against stored baseline |
