#!/bin/bash
set -e
cmake -S . -B build
cmake --build build -j
./build/uuv_sim Maps/test1/Harbour_Depth_Area.shp 100
./myEnv/bin/python3 visulaize.py