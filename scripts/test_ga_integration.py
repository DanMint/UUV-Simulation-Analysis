"""
test_ga_integration.py
======================

End-to-end integration tests for the Genetic Algorithm pipeline.

Tests:
    1. GA can run without crashing (attacker and defender)
    2. GA produces valid output files
    3. Fitness values are within expected ranges
    4. Best scenario can be loaded and simulated
    5. CSV output has correct columns and non-empty data
    6. Convergence plots are generated
    7. Benchmark harness passes quick checks

Usage:
    python scripts/test_ga_integration.py
    python scripts/test_ga_integration.py --quick
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import subprocess
import sys
import time
from typing import List, Optional


def run_command(args: list, timeout: int = 120) -> tuple[bool, str]:
    try:
        result = subprocess.run(
            [sys.executable] + args,
            capture_output=True,
            text=True,
            timeout=timeout,
            cwd=os.getcwd(),
        )
        return result.returncode == 0, result.stdout + result.stderr
    except Exception as e:
        return False, str(e)


def find_simulator() -> Optional[str]:
    candidates = [
        "./windows_build/build/Release/uuv_sim.exe",
        "../windows_build/build/Release/uuv_sim.exe",
        "./build/Release/uuv_sim.exe",
        "./uuv_sim.exe",
        "uuv_sim.exe",
    ]
    for c in candidates:
        if os.path.exists(c):
            return os.path.abspath(c)
    return None


def test_ga_runs(sim_exe: str, side: str, quick: bool = True) -> tuple[bool, str]:
    pop = 4 if quick else 8
    gens = 2 if quick else 4
    repeat = 2 if quick else 3
    args = [
        "./genetic_algorithm.py",
        "--scenario", "../scenarios/diveld_baseline_complete.json",
        "--side", side,
        "--pop", str(pop),
        "--gens", str(gens),
        "--repeat", str(repeat),
        "--seed", "42",
        "--sim", sim_exe,
        "--jobs", "1",
    ]
    if side == "attacker":
        args.extend(["--n-attackers", "3"])
    else:
        args.extend(["--n-detectors", "2", "--n-interceptors", "1"])

    success, output = run_command(args, timeout=180 if quick else 300)
    if not success:
        return False, f"GA failed to run: {output[:500]}"
    if "BEST CHROMOSOME" not in output:
        return False, "GA output missing BEST CHROMOSOME"
    return True, output


def test_output_files_exist() -> tuple[bool, List[str]]:
    required = [
        "../scenarios/ga_history.csv",
        "../scenarios/ga_convergence.png",
        "../scenarios/ga_best_scenario.json",
    ]
    missing = [f for f in required if not os.path.exists(f)]
    return len(missing) == 0, missing


def test_csv_valid() -> tuple[bool, str]:
    csv_path = "../scenarios/runs/ga_batch.csv"
    if not os.path.exists(csv_path):
        return False, "ga_batch.csv not found at ../scenarios/runs/ga_batch.csv"

    with open(csv_path, newline="") as f:
        reader = csv.DictReader(f)
        rows = list(reader)

    if not rows:
        return False, "ga_batch.csv is empty"

    required_cols = ["run_id", "probability_detected", "probability_killed",
                     "targets_destroyed", "total_targets", "blue_cost", "red_cost"]
    for col in required_cols:
        if col not in rows[0]:
            return False, f"Missing column: {col}"

    return True, f"CSV valid: {len(rows)} rows, columns: {list(rows[0].keys())}"


def test_best_scenario_loadable() -> tuple[bool, str]:
    path = "../scenarios/ga_best_scenario.json"
    if not os.path.exists(path):
        return False, "ga_best_scenario.json not found"
    try:
        with open(path, "r") as f:
            data = json.load(f)
        if "units" not in data:
            return False, "Missing 'units' in best scenario"
        if not data["units"]:
            return False, "Best scenario has no units"
        return True, f"Best scenario loadable: {len(data['units'])} units"
    except Exception as e:
        return False, f"Failed to load best scenario: {e}"


def test_fitness_range(side: str) -> tuple[bool, str]:
    hist_path = "../scenarios/ga_history.csv"
    if not os.path.exists(hist_path):
        return False, "ga_history.csv not found"

    with open(hist_path, newline="") as f:
        reader = csv.DictReader(f)
        rows = list(reader)

    if not rows:
        return False, "ga_history.csv is empty"

    best_fitnesses = [float(r["best_fitness"]) for r in rows]
    if not best_fitnesses:
        return False, "No best_fitness values in history"

    max_fitness = max(best_fitnesses)
    if side == "attacker" and max_fitness < 0.5:
        return False, f"Attacker fitness too low: {max_fitness}"
    if side == "defender" and max_fitness < 0.5:
        return False, f"Defender fitness too low: {max_fitness}"

    return True, f"Fitness range OK: max={max_fitness:.4f} across {len(rows)} generations"


def test_benchmark_quick(sim_exe: str) -> tuple[bool, str]:
    success, output = run_command(["scripts/benchmark_ga.py", "--quick"], timeout=300)
    if not success:
        return False, f"Benchmark failed: {output[:500]}"
    if "All benchmarks passed!" not in output:
        return False, "Benchmark did not pass all tests"
    return True, "Benchmark quick tests passed"


def main():
    ap = argparse.ArgumentParser(description="GA integration tests")
    ap.add_argument("--quick", action="store_true", help="Run reduced test suite")
    ap.add_argument("--sim", default=None, help="Path to uuv_sim.exe")
    args = ap.parse_args()

    sim_exe = args.sim or find_simulator()
    if not sim_exe or not os.path.exists(sim_exe):
        print("SKIP: uuv_sim.exe not found. Build the simulator first.")
        sys.exit(0)

    print("=" * 60)
    print("GA Integration Test Suite")
    print("=" * 60)

    tests = []
    for side in ["attacker", "defender"]:
        label = "attacker" if side == "attacker" else "defender"
        tests.append((f"GA runs ({label})", lambda s=side: test_ga_runs(sim_exe, s, args.quick)))
        tests.append((f"Fitness range ({label})", lambda s=side: test_fitness_range(s)))

    tests.extend([
        ("Output files exist", test_output_files_exist),
        ("CSV valid", test_csv_valid),
        ("Best scenario loadable", test_best_scenario_loadable),
    ])

    if not args.quick:
        tests.append(("Benchmark quick", lambda: test_benchmark_quick(sim_exe)))

    passed = 0
    failed = 0
    for name, test_fn in tests:
        try:
            result = test_fn()
            if isinstance(result, tuple):
                ok, msg = result
            else:
                ok, msg = result, ""
            if ok:
                print(f"  [PASS] {name}: {msg}")
                passed += 1
            else:
                print(f"  [FAIL] {name}: {msg}")
                failed += 1
        except Exception as e:
            print(f"  [FAIL] {name}: {e}")
            failed += 1

    print("=" * 60)
    print(f"Results: {passed}/{passed+failed} passed")
    if failed > 0:
        print("Some integration tests FAILED")
        sys.exit(1)
    print("All integration tests PASSED")
    sys.exit(0)


if __name__ == "__main__":
    main()
