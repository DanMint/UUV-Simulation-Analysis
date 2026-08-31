#!/bin/bash
set -e

cd "$(dirname "$0")"

./clean.sh || true

if [ -d "myenv" ] && [ ! -x "myenv/bin/python3" ]; then
	echo "Repairing Python environment..."
	python3 -m venv myenv
fi

if [ -x "myenv/bin/python3" ]; then
	PYTHON="myenv/bin/python3"
elif command -v python3 >/dev/null 2>&1; then
	PYTHON="$(command -v python3)"
else
	echo "Error: Python 3 is required for visualization and analysis."
	exit 1
fi

if ! "$PYTHON" -c 'import matplotlib, numpy, pandas, tabulate' >/dev/null 2>&1; then
	echo "Installing Python analysis dependencies..."
	"$PYTHON" -m pip install -r requirements.txt
fi

# Clean stale CMake cache
rm -rf build

# Building 
cmake -S . -B build
cmake --build build -j

# Run sim (creates Runs folder with json)
./build/uuv_sim Maps/pearlHarbour/Harbour_Depth_Area.shp 200

# Run visualizer (creates Paths winh PNGs)
$PYTHON visulaize.py

mkdir -p output

$PYTHON Analysis/IterationAnalysis.py runs/ output/

