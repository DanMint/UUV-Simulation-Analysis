#!/bin/bash
set -e

# Clean stale CMake cache (only needed once after CMakeLists changes)
rm -rf build

# Build
cmake -S . -B build
cmake --build build -j

# Run simulation (creates runs/ folder with results)
./build/uuv_sim Maps/pearlHarbour/Harbour_Depth_Area.shp 200

# Run visualizer using the venv's Python directly
./myEnv/bin/python3 visulaize.py