"""
analyze_sensitivity.py
======================

Parameter sensitivity analysis for the UUV simulator.

Varies one or more parameters across a range and measures the impact on
mission success, cost, and effectiveness. Helps identify which parameters
drive outcomes and whether the simulator is stable.

Usage:
    python scripts/analyze_sensitivity.py --scenario scenario.json --param sensing_radius --range 10,50 --steps 5

Supported parameters:
    sensing_radius   Detector sensing radius
    kill_radius      Interceptor kill radius
    noise_level      Environmental noise level
    n_detectors      Number of detectors
    n_interceptors   Number of interceptors

Requires: numpy, matplotlib, and a built uuv_sim.exe
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import subprocess
import sys
import tempfile
from typing import List, Optional, Dict, Any


def find_simulator() -> Optional[str]:
    candidates = [
        os.path.join(os.getcwd(), "windows_build", "build", "Release", "uuv_sim.exe"),
        os.path.join(os.getcwd(), "build", "Release", "uuv_sim.exe"),
        os.path.join(os.getcwd(), "uuv_sim.exe"),
        "uuv_sim.exe",
    ]
    for c in candidates:
        if os.path.exists(c):
            return os.path.abspath(c)
    return None


def run_simulator(sim_exe: str, scenario_path: str, repeat: int, seed: int, workdir: str) -> List[dict]:
    csv_path = os.path.join(workdir, "runs", "sensitivity_batch.csv")
    os.makedirs(os.path.dirname(csv_path), exist_ok=True)
    if os.path.exists(csv_path):
        os.remove(csv_path)

    cmd = [sim_exe, "--scenario", os.path.abspath(scenario_path), "--repeat", str(repeat),
           "--seed", str(seed), "--no-prompt"]

    env = os.environ.copy()
    gdal_bin = r"C:\gdal\bin"
    vcpkg_bin = r"C:\vcpkg\installed\x64-windows\bin"
    release_dir = os.path.abspath(os.path.dirname(sim_exe))
    env["PATH"] = gdal_bin + os.pathsep + vcpkg_bin + os.pathsep + release_dir + os.pathsep + env.get("PATH", "")

    subprocess.run(cmd, cwd=workdir, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                   timeout=300, env=env)

    rows = []
    if os.path.exists(csv_path):
        with open(csv_path, newline="") as f:
            for row in csv.DictReader(f):
                rows.append({k: float(row.get(k, 0.0)) for k in row})
    return rows


def vary_parameter(sim_exe: str, scenario_path: str, param: str, values: List[float],
                   repeat: int, seed: int, workdir: str) -> List[Dict[str, Any]]:
    results = []
    base = load_scenario(scenario_path)

    for val in values:
        sc = copy.deepcopy(base)
        if param == "sensing_radius":
            for u in sc.get("units", []):
                if u.get("type") == "detector":
                    u["sensing_radius"] = val
        elif param == "kill_radius":
            for u in sc.get("units", []):
                if u.get("type") == "interceptor":
                    u["kill_radius"] = val
        elif param == "noise_level":
            sc["noiseLevel"] = val
        elif param == "n_detectors":
            dets = [u for u in sc.get("units", []) if u.get("type") == "detector"]
            while len(dets) < val:
                dets.append({"type": "detector", "row": 0, "col": 0})
            sc["units"] = [u for u in sc.get("units", []) if u.get("type") != "detector"]
            sc["units"].extend(dets[:int(val)])
        elif param == "n_interceptors":
            ints = [u for u in sc.get("units", []) if u.get("type") == "interceptor"]
            while len(ints) < val:
                ints.append({"type": "interceptor", "row": 0, "col": 0})
            sc["units"] = [u for u in sc.get("units", []) if u.get("type") != "interceptor"]
            sc["units"].extend(ints[:int(val)])

        with tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False, dir=workdir) as f:
            json.dump(sc, f)
            temp_path = f.name

        try:
            rows = run_simulator(sim_exe, temp_path, repeat, seed, workdir)
            if rows:
                avg = {k: sum(r[k] for r in rows) / len(rows) for k in rows[0]}
                avg[param] = val
                results.append(avg)
        finally:
            os.unlink(temp_path)

    return results


def load_scenario(path: str) -> dict:
    with open(path, "r") as f:
        return json.load(f)


def report(results: List[Dict[str, Any]], param: str, outdir: str) -> None:
    if not results:
        return

    vals = [r[param] for r in results]
    success = [r.get("mission_success_rate", 0.0) for r in results]
    blue = [r.get("blue_cost", 0.0) for r in results]
    red = [r.get("red_cost", 0.0) for r in results]
    ler = [r.get("loss_exchange_ratio", 0.0) for r in results]
    pdet = [r.get("probability_detected", 0.0) for r in results]
    pkill = [r.get("probability_killed", 0.0) for r in results]

    import matplotlib.pyplot as plt
    import numpy as np

    fig, axes = plt.subplots(2, 2, figsize=(12, 10))
    fig.suptitle(f"Sensitivity: {param} vs Outcomes")

    axes[0, 0].plot(vals, success, "o-", color="#1E64DC")
    axes[0, 0].set_xlabel(param)
    axes[0, 0].set_ylabel("Mission success rate")
    axes[0, 0].grid(True, alpha=0.3)
    axes[0, 0].set_title("Success Rate")

    axes[0, 1].plot(vals, pdet, "o-", label="P(detected)", color="#E85D04")
    axes[0, 1].plot(vals, pkill, "o-", label="P(killed)", color="#A050FF")
    axes[0, 1].set_xlabel(param)
    axes[0, 1].set_ylabel("Probability")
    axes[0, 1].legend()
    axes[0, 1].grid(True, alpha=0.3)
    axes[0, 1].set_title("Detection / Kill Probabilities")

    axes[1, 0].plot(vals, blue, "o-", label="Blue cost", color="#1E64DC")
    axes[1, 0].plot(vals, red, "o-", label="Red cost", color="#E85D04")
    axes[1, 0].set_xlabel(param)
    axes[1, 0].set_ylabel("Cost ($)")
    axes[1, 0].legend()
    axes[1, 0].grid(True, alpha=0.3)
    axes[1, 0].set_title("Cost Trade-off")

    axes[1, 1].plot(vals, ler, "o-", color="#A050FF")
    axes[1, 1].axhline(1.0, color="k", linestyle="--", alpha=0.5)
    axes[1, 1].set_xlabel(param)
    axes[1, 1].set_ylabel("Loss exchange ratio")
    axes[1, 1].grid(True, alpha=0.3)
    axes[1, 1].set_title("Break-even Analysis")

    plt.tight_layout()
    out = os.path.join(outdir, f"sensitivity_{param}.png")
    plt.savefig(out, dpi=150)
    plt.close()
    print(f"Saved: {out}")

    # CSV
    csv_path = os.path.join(outdir, f"sensitivity_{param}.csv")
    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=sorted({k for r in results for k in r}))
        writer.writeheader()
        writer.writerows(results)
    print(f"Saved: {csv_path}")


def main():
    ap = argparse.ArgumentParser(description="UUV parameter sensitivity analysis")
    ap.add_argument("--scenario", required=True, help="Path to scenario.json")
    ap.add_argument("--param", required=True,
                    choices=["sensing_radius", "kill_radius", "noise_level", "n_detectors", "n_interceptors"])
    ap.add_argument("--range", required=True, help="Comma-separated min,max")
    ap.add_argument("--steps", type=int, default=5)
    ap.add_argument("--repeat", type=int, default=3)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--sim", default=None, help="Path to uuv_sim.exe")
    ap.add_argument("--outdir", default=None)
    args = ap.parse_args()

    sim_exe = args.sim or find_simulator()
    if not sim_exe or not os.path.exists(sim_exe):
        print("ERROR: uuv_sim.exe not found. Pass --sim or build the simulator.")
        sys.exit(2)

    workdir = os.path.dirname(os.path.abspath(args.scenario))
    outdir = args.outdir or workdir
    os.makedirs(outdir, exist_ok=True)

    lo, hi = map(float, args.range.split(","))
    values = [lo + i * (hi - lo) / max(args.steps - 1, 1) for i in range(args.steps)]

    print(f"Running sensitivity analysis: {args.param} in [{lo}, {hi}] ({args.steps} steps, repeat={args.repeat})")
    results = vary_parameter(sim_exe, args.scenario, args.param, values, args.repeat, args.seed, workdir)
    report(results, args.param, outdir)
    print("Done.")


if __name__ == "__main__":
    main()
