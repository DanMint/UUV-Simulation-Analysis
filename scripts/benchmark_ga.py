"""
benchmark_ga.py
===============

Benchmark harness for the UUV Genetic Algorithm.

Runs a standardized set of GA configurations and validates that results
meet minimum quality criteria. Designed for CI/regression testing.

Usage:
    python scripts/benchmark_ga.py [--sim path/to/uuv_sim.exe] [--quick]

Modes:
    --quick    Run reduced-size benchmarks (fast CI check)
    --full     Run full benchmarks (comprehensive validation)

Exit codes:
    0  All benchmarks passed
    1  One or more benchmarks failed
    2  Setup error (missing simulator, etc.)
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from dataclasses import dataclass, field
from typing import List, Optional

# ─────────────────────────────────────────────────────────────────────────────
#  Benchmark definitions
# ─────────────────────────────────────────────────────────────────────────────

SCENARIO = "scenarios/diveld_baseline_complete.json"

QUICK_BENCHMARKS = [
    {
        "name": "attacker_quick",
        "side": "attacker",
        "pop": 4,
        "gens": 3,
        "repeat": 2,
        "seed": 42,
        "n_attackers": 3,
        "min_fitness": 0.8,
        "min_destroy_rate": 0.8,
        "max_time_s": 60,
    },
    {
        "name": "defender_quick",
        "side": "defender",
        "pop": 4,
        "gens": 3,
        "repeat": 2,
        "seed": 42,
        "n_detectors": 2,
        "n_interceptors": 1,
        "min_fitness": 0.8,
        "min_effectiveness": 0.8,
        "max_time_s": 60,
    },
]

FULL_BENCHMARKS = [
    {
        "name": "attacker_full",
        "side": "attacker",
        "pop": 10,
        "gens": 5,
        "repeat": 3,
        "seed": 42,
        "n_attackers": 3,
        "min_fitness": 0.9,
        "min_destroy_rate": 0.9,
        "max_time_s": 120,
    },
    {
        "name": "defender_full",
        "side": "defender",
        "pop": 10,
        "gens": 5,
        "repeat": 3,
        "seed": 42,
        "n_detectors": 2,
        "n_interceptors": 1,
        "min_fitness": 0.9,
        "min_effectiveness": 0.9,
        "max_time_s": 120,
    },
    {
        "name": "attacker_islands",
        "side": "attacker",
        "pop": 8,
        "gens": 4,
        "repeat": 2,
        "seed": 42,
        "n_attackers": 3,
        "islands": 2,
        "jobs": 1,
        "min_fitness": 0.85,
        "min_destroy_rate": 0.85,
        "max_time_s": 120,
    },
]


@dataclass
class BenchmarkResult:
    name: str
    passed: bool
    fitness: float = 0.0
    destroy_rate: float = 0.0
    effectiveness: float = 0.0
    red_cost: float = 0.0
    elapsed_s: float = 0.0
    error: Optional[str] = None
    details: List[str] = field(default_factory=list)


def run_ga(args: list) -> tuple[bool, dict]:
    """Run GA and return (success, result_dict)."""
    try:
        result = subprocess.run(
            [sys.executable, "scripts/genetic_algorithm.py"] + args,
            capture_output=True,
            text=True,
            timeout=180,
            cwd=os.getcwd(),
        )
        return result.returncode == 0, {"returncode": result.returncode, "stdout": result.stdout, "stderr": result.stderr}
    except subprocess.TimeoutExpired:
        return False, {"error": "Timeout"}
    except Exception as e:
        return False, {"error": str(e)}


def parse_ga_output(stdout: str) -> dict:
    """Parse key metrics from GA stdout."""
    metrics = {}
    for line in stdout.split("\n"):
        if "Fitness:" in line:
            try:
                metrics["fitness"] = float(line.split("Fitness:")[1].strip().split()[0])
            except (IndexError, ValueError):
                pass
        elif "Target destruction rate:" in line:
            try:
                metrics["destroy_rate"] = float(line.split("Target destruction rate:")[1].strip().split()[0])
            except (IndexError, ValueError):
                pass
        elif "P(detected)*P(killed):" in line:
            try:
                metrics["effectiveness"] = float(line.split("P(detected)*P(killed):")[1].strip().split()[0])
            except (IndexError, ValueError):
                pass
        elif "Attacker cost (red):" in line:
            try:
                metrics["red_cost"] = float(line.split("Attacker cost (red):")[1].strip().split()[0])
            except (IndexError, ValueError):
                pass
        elif "Deployment cost:" in line:
            try:
                metrics["deploy_cost"] = float(line.split("Deployment cost:")[1].strip().split()[0])
            except (IndexError, ValueError):
                pass
    return metrics


def run_benchmark(bm: dict, sim_exe: str) -> BenchmarkResult:
    """Run a single GA benchmark and validate results."""
    name = bm["name"]
    side = bm["side"]
    start = time.time()

    args = [
        "--scenario", SCENARIO,
        "--side", side,
        "--pop", str(bm["pop"]),
        "--gens", str(bm["gens"]),
        "--repeat", str(bm["repeat"]),
        "--seed", str(bm["seed"]),
        "--sim", sim_exe,
        "--jobs", str(bm.get("jobs", 1)),
    ]

    if side == "attacker":
        args.extend(["--n-attackers", str(bm["n_attackers"])])
    else:
        args.extend(["--n-detectors", str(bm["n_detectors"]), "--n-interceptors", str(bm["n_interceptors"])])

    if "islands" in bm:
        args.extend(["--islands", str(bm["islands"])])

    success, result_info = run_ga(args)
    elapsed = time.time() - start

    br = BenchmarkResult(name=name, passed=False, elapsed_s=elapsed)

    if not success:
        br.error = result_info.get("error", f"GA failed with return code {result_info.get('returncode', 'unknown')}")
        br.details.append(br.error)
        return br

    stdout = result_info.get("stdout", "")
    metrics = parse_ga_output(stdout)
    br.fitness = metrics.get("fitness", 0.0)
    br.destroy_rate = metrics.get("destroy_rate", 0.0)
    br.effectiveness = metrics.get("effectiveness", 0.0)
    br.red_cost = metrics.get("red_cost", 0.0)

    checks = []
    if "min_fitness" in bm:
        check = br.fitness >= bm["min_fitness"]
        checks.append(check)
        br.details.append(f"fitness {br.fitness:.4f} >= {bm['min_fitness']}: {'PASS' if check else 'FAIL'}")
    if side == "attacker" and "min_destroy_rate" in bm:
        check = br.destroy_rate >= bm["min_destroy_rate"]
        checks.append(check)
        br.details.append(f"destroy_rate {br.destroy_rate:.4f} >= {bm['min_destroy_rate']}: {'PASS' if check else 'FAIL'}")
    if side == "defender" and "min_effectiveness" in bm:
        check = br.effectiveness >= bm["min_effectiveness"]
        checks.append(check)
        br.details.append(f"effectiveness {br.effectiveness:.4f} >= {bm['min_effectiveness']}: {'PASS' if check else 'FAIL'}")
    if "max_time_s" in bm:
        check = elapsed <= bm["max_time_s"]
        checks.append(check)
        br.details.append(f"time {elapsed:.1f}s <= {bm['max_time_s']}s: {'PASS' if check else 'FAIL'}")

    br.passed = all(checks)
    return br


def find_simulator() -> Optional[str]:
    """Locate uuv_sim.exe."""
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


def main():
    ap = argparse.ArgumentParser(description="GA benchmark harness")
    ap.add_argument("--sim", default=None, help="Path to uuv_sim.exe")
    ap.add_argument("--quick", action="store_true", help="Run quick benchmarks only")
    ap.add_argument("--full", action="store_true", help="Run full benchmarks")
    args = ap.parse_args()

    sim_exe = args.sim or find_simulator()
    if not sim_exe or not os.path.exists(sim_exe):
        print(f"ERROR: uuv_sim.exe not found. Pass --sim or build the simulator.")
        sys.exit(2)

    print(f"Using simulator: {sim_exe}")
    benchmarks = FULL_BENCHMARKS if args.full else QUICK_BENCHMARKS
    if args.quick:
        benchmarks = QUICK_BENCHMARKS

    print(f"\nRunning {len(benchmarks)} GA benchmarks...")
    print("=" * 60)

    results: List[BenchmarkResult] = []
    for bm in benchmarks:
        print(f"\n[Benchmark] {bm['name']}...")
        result = run_benchmark(bm, sim_exe)
        results.append(result)
        for detail in result.details:
            print(f"  {detail}")
        status = "PASSED" if result.passed else "FAILED"
        print(f"  Result: {status} (fitness={result.fitness:.4f}, time={result.elapsed_s:.1f}s)")

    print("\n" + "=" * 60)
    passed = sum(1 for r in results if r.passed)
    total = len(results)
    print(f"Benchmarks: {passed}/{total} passed")

    if passed < total:
        print("\nFailed benchmarks:")
        for r in results:
            if not r.passed:
                print(f"  - {r.name}: {r.error}")
        sys.exit(1)

    print("All benchmarks passed!")
    sys.exit(0)


if __name__ == "__main__":
    main()
