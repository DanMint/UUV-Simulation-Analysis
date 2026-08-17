"""
sensitivity_analysis.py
=======================

Parameter sensitivity analysis for the UUV Genetic Algorithm.

Runs the GA with varying parameters and plots how fitness changes.
Helps identify which parameters have the most impact on performance.

Usage:
    python scripts/sensitivity_analysis.py --param population_size --values 4,8,16,32
    python scripts/sensitivity_analysis.py --param mutation_rate --values 0.05,0.1,0.2,0.3
    python scripts/sensitivity_analysis.py --param lambda --values 0.1,0.3,0.5,1.0

Supported parameters:
    population_size  --pop
    mutation_rate    --pm
    lambda           --lambda
    repeat           --repeat
    generations      --gens
"""

from __future__ import annotations

import argparse
import csv
import os
import subprocess
import sys
import time
from typing import List, Optional

import matplotlib.pyplot as plt
import numpy as np


def run_ga_single(param_overrides: dict, sim_exe: str, base_args: dict) -> Optional[dict]:
    """Run GA with parameter overrides and return parsed metrics."""
    args = [
        sys.executable, "scripts/genetic_algorithm.py",
        "--scenario", base_args["scenario"],
        "--side", base_args["side"],
        "--pop", str(param_overrides.get("pop", base_args.get("pop", 8))),
        "--gens", str(param_overrides.get("gens", base_args.get("gens", 3))),
        "--repeat", str(param_overrides.get("repeat", base_args.get("repeat", 2))),
        "--seed", str(base_args.get("seed", 42)),
        "--pm", str(param_overrides.get("pm", base_args.get("pm", 0.15))),
        "--lambda", str(param_overrides.get("lambda", base_args.get("lambda", 0.5))),
        "--sim", sim_exe,
        "--jobs", "1",
    ]

    if base_args["side"] == "attacker":
        args.extend(["--n-attackers", str(base_args.get("n_attackers", 3))])
    else:
        args.extend(["--n-detectors", str(base_args.get("n_detectors", 2))])
        args.extend(["--n-interceptors", str(base_args.get("n_interceptors", 1))])

    try:
        result = subprocess.run(args, capture_output=True, text=True, timeout=120, cwd=os.getcwd())
        if result.returncode != 0:
            return None

        metrics = {}
        for line in result.stdout.split("\n"):
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
            elif "time=" in line and "Gen" in line:
                try:
                    time_str = line.split("time=")[1].split("s")[0].strip()
                    metrics["gen_time_s"] = float(time_str)
                except (IndexError, ValueError):
                    pass
        return metrics
    except Exception:
        return None


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


def main():
    ap = argparse.ArgumentParser(description="GA parameter sensitivity analysis")
    ap.add_argument("--param", required=True,
                    choices=["population_size", "mutation_rate", "lambda", "repeat", "generations"],
                    help="Parameter to analyze")
    ap.add_argument("--values", required=True, help="Comma-separated values to test")
    ap.add_argument("--sim", default=None, help="Path to uuv_sim.exe")
    ap.add_argument("--side", default="attacker", choices=["attacker", "defender"])
    ap.add_argument("--scenario", default="scenarios/diveld_baseline_complete.json")
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--outdir", default="scenarios", help="Output directory for plots")
    args = ap.parse_args()

    sim_exe = args.sim or find_simulator()
    if not sim_exe or not os.path.exists(sim_exe):
        print(f"ERROR: uuv_sim.exe not found. Pass --sim or build the simulator.")
        sys.exit(2)

    values = [float(v.strip()) if "." in v.strip() else int(v.strip()) for v in args.values.split(",")]
    print(f"Sensitivity analysis: {args.param} = {values}")
    print(f"Side: {args.side}, Scenario: {args.scenario}")
    print("=" * 60)

    base_args = {
        "scenario": args.scenario,
        "side": args.side,
        "pop": 8,
        "gens": 3,
        "repeat": 2,
        "seed": args.seed,
        "pm": 0.15,
        "lambda": 0.5,
        "n_attackers": 3,
        "n_detectors": 2,
        "n_interceptors": 1,
    }

    param_map = {
        "population_size": "pop",
        "mutation_rate": "pm",
        "lambda": "lambda",
        "repeat": "repeat",
        "generations": "gens",
    }
    param_key = param_map[args.param]

    results = []
    for v in values:
        print(f"\nTesting {args.param} = {v}...")
        overrides = {param_key: v}
        metrics = run_ga_single(overrides, sim_exe, base_args)
        if metrics:
            results.append((v, metrics))
            fitness = metrics.get("fitness", 0.0)
            print(f"  fitness={fitness:.4f}")
        else:
            results.append((v, {}))
            print("  FAILED")

    # ── Plot results ─────────────────────────────────────────────────
    if not results:
        print("No results to plot.")
        return

    xs = [r[0] for r in results]
    fitness_vals = [r[1].get("fitness", 0.0) for r in results]

    plt.figure(figsize=(10, 6))
    plt.plot(xs, fitness_vals, "o-", linewidth=2, markersize=8, color="#1E64DC")
    plt.xlabel(args.param.replace("_", " ").title())
    plt.ylabel("Best Fitness")
    plt.title(f"GA Sensitivity: {args.param} ({args.side} optimizer)")
    plt.grid(True, alpha=0.3)
    plt.tight_layout()

    out_path = os.path.join(args.outdir, f"ga_sensitivity_{args.param}.png")
    plt.savefig(out_path, dpi=150)
    plt.close()
    print(f"\nSaved sensitivity plot: {out_path}")

    # ── Save CSV ─────────────────────────────────────────────────────
    csv_path = os.path.join(args.outdir, f"ga_sensitivity_{args.param}.csv")
    with open(csv_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([args.param, "fitness", "destroy_rate", "effectiveness", "red_cost", "deploy_cost"])
        for v, m in results:
            writer.writerow([
                v,
                m.get("fitness", 0.0),
                m.get("destroy_rate", 0.0),
                m.get("effectiveness", 0.0),
                m.get("red_cost", 0.0),
                m.get("deploy_cost", 0.0),
            ])
    print(f"Saved sensitivity data: {csv_path}")


if __name__ == "__main__":
    main()
