# Developer Guide

## Building the Project

### Prerequisites
- CMake 3.20+
- C++17 compiler (MSVC 2019+, GCC 9+, Clang 10+)
- SFML 3.0+ (Graphics, Window, System)
- GDAL 3.0+ (for shapefile support)
- Python 3.8+ (for GA tools and scripts)

### Windows (MSVC)
```powershell
# Configure
cmake -B windows_build\build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ..

# Build
cmake --build windows_build\build --config Release

# Test
ctest --test-dir windows_build\build --output-on-failure
```

### Linux (GCC/Clang)
```bash
# Install dependencies
sudo apt-get install libsfml-dev libgdal-dev cmake build-essential

# Configure and build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Test
ctest --test-dir build --output-on-failure
```

### macOS
```bash
# Install dependencies
brew install sfml gdal cmake

# Configure and build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Project Structure

```
UUV-Simulation-Analysis/
├── src/
│   ├── agents/           # Agent implementations (seeker, target, detector, interceptor, attacker)
│   ├── mapCreation/      # Map/grid management and shapefile loading
│   ├── spawnConfig/      # Scenario configuration and JSON I/O
│   ├── pathfinding/      # A* pathfinding with Octile heuristic
│   ├── simulation/       # Main simulation engine
│   ├── simulationVisualizer/  # SFML-based visualization
│   ├── mapVisualizer/    # Map rendering
│   └── utils/            # Utilities (logger, spatial grid)
├── tests/                # C++ unit tests
├── scripts/              # Python tools (GA, analysis, CLI)
├── scenarios/            # Scenario JSON/YAML files
├── docs/                 # Documentation
├── CMakeLists.txt        # Root CMake configuration
└── windows_build/        # Windows-specific build files
```

## Architecture

### Simulation Engine
The simulation runs in discrete steps. Each step:
1. Seekers move along A* paths
2. Environmental noise displaces agents
3. Detectors update tracks (sense-then-shoot doctrine)
4. Interceptors engage detected targets
5. Collision detection
6. Termination checks

### Agent Types
- **Seeker**: Pathfinds to nearest target, can be detected and intercepted
- **Target**: Static objective, can be destroyed by seekers/attackers
- **Detector**: Passive sensor, marks agents as detected
- **Interceptor**: Active effector, kills detected agents within range
- **Attacker**: Advanced agent with FSM lifecycle, real-world vehicle specs

### GA Integration
The Python GA tools call the C++ simulator via subprocess (or direct batch API). Each evaluation:
1. Generates a scenario from chromosome
2. Runs simulation with repeat/seed for statistical robustness
3. Extracts metrics (fitness, cost, effectiveness)
4. Returns fitness value to GA optimizer

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make changes with tests
4. Run `ctest` to ensure all tests pass
5. Submit a pull request

## Performance Optimization

The simulation engine has been optimized with:
- Squared-distance range checks (eliminates redundant sqrt calls)
- Flat-buffer A* (eliminates per-call heap allocations)
- Spatial hashing for detector/interceptor queries
- Pre-allocated vectors with reserved capacity
- Pre-constructed RNG distributions

Benchmark results: ~1000-2000 steps/second on modern hardware.
