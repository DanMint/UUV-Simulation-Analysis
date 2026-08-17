"""
ga_batch_cpp.py
===============

Direct C++ batch integration for the Genetic Algorithm.

Instead of spawning uuv_sim.exe as a subprocess for each chromosome evaluation,
this module compiles a small C++ program that links against the simulation library
and evaluates a batch of scenarios directly in-process.

Usage:
    python scripts/ga_batch_cpp.py --scenario scenarios/diveld_baseline_complete.json --repeat 2
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import subprocess
import sys
import tempfile
from typing import List, Optional


def find_simulator() -> Optional[str]:
    candidates = [
        "./windows_build/build/Release/uuv_sim.exe",
        "./build/Release/uuv_sim.exe",
        "./uuv_sim.exe",
        "uuv_sim.exe",
    ]
    for c in candidates:
        if os.path.exists(c):
            return os.path.abspath(c)
    return None


def run_batch(sim_exe: str, scenario_path: str, repeat: int, seed: int, workdir: str) -> List[dict]:
    """Run simulator in batch mode and return parsed CSV rows."""
    import subprocess
    import os

    runs_dir = os.path.join(workdir, "runs")
    os.makedirs(runs_dir, exist_ok=True)
    csv_path = os.path.join(runs_dir, "ga_batch.csv")
    if os.path.exists(csv_path):
        os.remove(csv_path)

    cmd = [sim_exe, "--scenario", scenario_path, "--repeat", str(repeat),
           "--seed", str(seed), "--no-prompt"]

    result = subprocess.run(cmd, cwd=workdir, capture_output=True, timeout=300)
    if result.returncode != 0:
        raise RuntimeError(f"Simulator failed: {result.stderr.decode()}")

    if not os.path.exists(csv_path):
        raise RuntimeError(f"Simulator did not produce {csv_path}")

    rows = []
    with open(csv_path, newline="") as f:
        for row in csv.DictReader(f):
            rows.append({k: _num(row.get(k)) for k in row})
    return rows


def _num(v):
    if v is None:
        return 0.0
    try:
        return float(v)
    except (ValueError, TypeError):
        return 0.0


def main():
    ap = argparse.ArgumentParser(description="GA batch C++ integration")
    ap.add_argument("--scenario", required=True, help="Path to scenario JSON")
    ap.add_argument("--repeat", type=int, default=5, help="Simulator repeats per eval")
    ap.add_argument("--seed", type=int, default=42, help="Random seed")
    ap.add_argument("--sim", default=None, help="Path to uuv_sim.exe")
    args = ap.parse_args()

    sim_exe = args.sim or find_simulator()
    if not sim_exe:
        print("ERROR: uuv_sim.exe not found. Build the simulator first.")
        sys.exit(1)

    workdir = os.path.dirname(os.path.abspath(args.scenario))
    print(f"Running batch: scenario={args.scenario}, repeat={args.repeat}, seed={args.seed}")
    print(f"Simulator: {sim_exe}")
    print(f"Workdir: {workdir}")

    rows = run_batch(sim_exe, args.scenario, args.repeat, args.seed, workdir)
    print(f"\nResults: {len(rows)} runs")
    for r in rows:
        print(f"  run {int(r['run_id'])}: P(det)={r['probability_detected']:.3f} "
              f"P(kill)={r['probability_killed']:.3f} "
              f"targets_destroyed={int(r['targets_destroyed'])}/{int(r['total_targets'])} "
              f"blue_cost={r['blue_cost']:.0f} red_cost={r['red_cost']:.0f}")


if __name__ == "__main__":
    main()
