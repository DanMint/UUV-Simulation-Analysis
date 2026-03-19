#!/bin/bash
set -e

# Clean stale CMake cache
rm -rf build

# Building 
cmake -S . -B build
cmake --build build -j

# Run sim (creates Runs folder with json)
./build/uuv_sim Maps/pearlHarbour/Harbour_Depth_Area.shp 200

# Run visualizer (creates Paths winh PNGs)
./myEnv/bin/python3 visulaize.py