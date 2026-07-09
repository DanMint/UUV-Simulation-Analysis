#!/bin/bash

export HF_TOKEN=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

echo 
rm -rf output/
rm -rf runs/*

echo 
./run.sh

echo 
python3 Analysis/DataAligner.py

echo "Evaluating Telemetry Analysis"
python3 Analysis/TelemetryAnalysis.py

echo 
python3 Analysis/IterationAnalysis.py runs/ output/

echo 
python3 Analysis/CbaAnalysis.py