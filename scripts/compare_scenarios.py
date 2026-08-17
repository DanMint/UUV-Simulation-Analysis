"""
compare_scenarios.py
====================

Scenario comparison and regression testing tool.

Compares two scenario JSON files or two run directories and reports
differences in metrics, unit placements, and outcomes.

Usage:
    python scripts/compare_scenarios.py scenario_a.json scenario_b.json
    python scripts/compare_scenarios.py runs_dir_a/ runs_dir_b/
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import sys
from typing import List, Optional


def load_scenario(path: str) -> dict:
    with open(path, "r") as f:
        return json.load(f)


def compare_scenarios(a: dict, b: dict) -> List[str]:
    diffs = []
    for key in ["grid", "detector_radius", "interceptor_radius", "max_noise_level"]:
        if a.get(key) != b.get(key):
            diffs.append(f"{key}: A={a.get(key)} vs B={b.get(key)}")

    units_a = {(u.get("type"), u.get("row"), u.get("col")) for u in a.get("units", [])}
    units_b = {(u.get("type"), u.get("row"), u.get("col")) for u in b.get("units", [])}
    only_a = units_a - units_b
    only_b = units_b - units_a
    if only_a:
        diffs.append(f"Units only in A: {only_a}")
    if only_b:
        diffs.append(f"Units only in B: {only_b}")

    return diffs


def compare_runs(dir_a: str, dir_b: str) -> List[str]:
    diffs = []
    csv_a = os.path.join(dir_a, "ga_batch.csv")
    csv_b = os.path.join(dir_b, "ga_batch.csv")

    if not os.path.exists(csv_a) or not os.path.exists(csv_b):
        return ["Missing ga_batch.csv in one or both directories"]

    with open(csv_a, newline="") as f:
        rows_a = list(csv.DictReader(f))
    with open(csv_b, newline="") as f:
        rows_b = list(csv.DictReader(f))

    if len(rows_a) != len(rows_b):
        diffs.append(f"Row count differs: A={len(rows_a)}, B={len(rows_b)}")

    for i, (ra, rb) in enumerate(zip(rows_a, rows_b)):
        for key in ["probability_detected", "probability_killed", "targets_destroyed",
                    "total_targets", "blue_cost", "red_cost", "effectiveness"]:
            va = float(ra.get(key, 0))
            vb = float(rb.get(key, 0))
            if abs(va - vb) > 0.01:
                diffs.append(f"Row {i} {key}: A={va:.4f}, B={vb:.4f}")

    return diffs


def main():
    ap = argparse.ArgumentParser(description="Compare UUV scenarios or run results")
    ap.add_argument("path_a", help="First scenario JSON or runs directory")
    ap.add_argument("path_b", help="Second scenario JSON or runs directory")
    args = ap.parse_args()

    if os.path.isdir(args.path_a) and os.path.isdir(args.path_b):
        print("Comparing run directories...")
        diffs = compare_runs(args.path_a, args.path_b)
    elif os.path.isfile(args.path_a) and os.path.isfile(args.path_b):
        print("Comparing scenario files...")
        a = load_scenario(args.path_a)
        b = load_scenario(args.path_b)
        diffs = compare_scenarios(a, b)
    else:
        print("ERROR: Both paths must be files or both must be directories.")
        sys.exit(1)

    if diffs:
        print("Differences found:")
        for d in diffs:
            print(f"  - {d}")
        sys.exit(1)
    else:
        print("No differences found. Scenarios are identical.")


if __name__ == "__main__":
    main()
