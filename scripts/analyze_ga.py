"""
analyze_ga.py
=============

Analysis tools for Genetic Algorithm runs.

Reads `ga_history.csv` (produced by genetic_algorithm.py) and `runs/ga_batch.csv`
to produce:
  - Fitness convergence plot with diversity overlay
  - Pareto frontier for multi-objective analysis
  - Cost-effectiveness scatter (defender: effectiveness vs deployment cost)
  - Vehicle-type breakdown across generations
  - Summary statistics

Usage:
    python scripts/analyze_ga.py [workdir]

Requires: numpy, matplotlib, pandas (optional)
"""

import csv
import json
import os
import sys

import matplotlib.pyplot as plt
import numpy as np


def load_ga_history(workdir: str):
    path = os.path.join(workdir, "ga_history.csv")
    if not os.path.exists(path):
        print(f"No ga_history.csv found in {workdir}")
        return []
    rows = []
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            rows.append({k: _num(row.get(k)) for k, v in row.items()})
            if "generation" in rows[-1]:
                rows[-1]["generation"] = int(rows[-1]["generation"])
    return rows


def load_ga_batch(workdir: str):
    path = os.path.join(workdir, "ga_batch.csv")
    if not os.path.exists(path):
        return []
    rows = []
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            rows.append({k: float(v) if k != "run_id" else v for k, v in row.items()})
    return rows


def load_best_scenario(workdir: str):
    path = os.path.join(workdir, "ga_best_scenario.json")
    if not os.path.exists(path):
        return None
    with open(path, "r") as f:
        return json.load(f)


def report_convergence(history, workdir: str):
    if not history:
        return
    gens = [h["generation"] for h in history]
    best = [h["best_fitness"] for h in history]
    avg = [h["avg_fitness"] for h in history]
    div = [h.get("diversity", 0.0) for h in history]

    fig, ax1 = plt.subplots(figsize=(10, 5))
    ax1.plot(gens, best, label="Best fitness", linewidth=2, color="#1E64DC")
    ax1.plot(gens, avg, label="Avg fitness", linewidth=1, alpha=0.7, color="#888")
    ax1.set_xlabel("Generation")
    ax1.set_ylabel("Fitness")
    ax1.set_title("GA Fitness Convergence")
    ax1.legend(loc="upper left")
    ax1.grid(True, alpha=0.3)

    if any(d > 0 for d in div):
        ax2 = ax1.twinx()
        ax2.plot(gens, div, label="Diversity", linewidth=1.5, color="#E85D04", linestyle="--")
        ax2.set_ylabel("Diversity (unique/pop)")
        ax2.legend(loc="upper right")

    plt.tight_layout()
    out = os.path.join(workdir, "ga_analyze_convergence.png")
    plt.savefig(out, dpi=150)
    plt.close()
    print(f"Saved: {out}")


def report_pareto(history, workdir: str):
    if not history:
        return
    eff = [h.get("best_effectiveness", 0.0) for h in history]
    cost = [h.get("best_deploy_cost", 0.0) for h in history]
    gen = [h["generation"] for h in history]

    fig, ax = plt.subplots(figsize=(8, 6))
    sc = ax.scatter(eff, cost, c=gen, cmap="viridis", s=80, edgecolor="k", linewidth=0.5)
    cb = fig.colorbar(sc, ax=ax)
    cb.set_label("Generation")
    ax.set_xlabel("Effectiveness (P(detected) * P(killed))")
    ax.set_ylabel("Deployment cost")
    ax.set_title("Pareto Frontier: Effectiveness vs Cost Over Generations")
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    out = os.path.join(workdir, "ga_analyze_pareto.png")
    plt.savefig(out, dpi=150)
    plt.close()
    print(f"Saved: {out}")


def report_best_scenario(best, workdir: str):
    if not best:
        return
    units = best.get("units", [])
    attackers = [u for u in units if u.get("type") == "attacker"]
    detectors = [u for u in units if u.get("type") == "detector"]
    interceptors = [u for u in units if u.get("type") == "interceptor"]

    print("\n" + "=" * 56)
    print("       BEST SCENARIO BREAKDOWN")
    print("=" * 56)
    print(f"Attackers   : {len(attackers)}")
    for a in attackers:
        print(f"  {a.get('vehicle_type', 'generic'):15s} at ({a['row']:>3}, {a['col']:>3})")
    print(f"Detectors   : {len(detectors)}")
    for d in detectors:
        print(f"  generic     at ({d['row']:>3}, {d['col']:>3})")
    print(f"Interceptors: {len(interceptors)}")
    for ic in interceptors:
        print(f"  generic     at ({ic['row']:>3}, {ic['col']:>3})")
    print("=" * 56)

    if attackers:
        import matplotlib.pyplot as plt
        from collections import Counter
        vtypes = [a.get("vehicle_type", "unknown") for a in attackers]
        counts = Counter(vtypes)
        fig, ax = plt.subplots(figsize=(8, 5))
        ax.bar(counts.keys(), counts.values(), color="#1E64DC", alpha=0.8)
        ax.set_xlabel("Vehicle type")
        ax.set_ylabel("Count")
        ax.set_title("Best Scenario: Attacker Vehicle Type Distribution")
        ax.tick_params(axis="x", rotation=45)
        ax.grid(True, alpha=0.3, axis="y")
        plt.tight_layout()
        out = os.path.join(workdir, "ga_best_vehicle_types.png")
        plt.savefig(out, dpi=150)
        plt.close()
        print(f"Saved: {out}")


def main():
    base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    workdir = sys.argv[1] if len(sys.argv) > 1 else base

    history = load_ga_history(workdir)
    if not history:
        print("No GA history found. Run genetic_algorithm.py first.")
        sys.exit(1)

    print(f"Analyzing GA run in: {workdir}")
    print(f"  Generations: {len(history)}")
    print(f"  Best fitness: {max(h['best_fitness'] for h in history):.4f}")

    report_convergence(history, workdir)
    report_pareto(history, workdir)

    best = load_best_scenario(workdir)
    report_best_scenario(best, workdir)


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] in ("--help", "-h"):
        print(__doc__.strip())
        sys.exit(0)
    main()
