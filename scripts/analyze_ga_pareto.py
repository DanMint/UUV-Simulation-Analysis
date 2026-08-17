"""
analyze_ga_pareto.py
====================

Analyze GA history CSV to produce Pareto frontier plots for multi-objective
analysis. Works with the output of genetic_algorithm.py (ga_history.csv and
ga_batch.csv).

Usage:
    python scripts/analyze_ga_pareto.py [--history ga_history.csv] [--batch runs/ga_batch.csv]
"""

from __future__ import annotations

import argparse
import csv
import os
from typing import List, Tuple

import matplotlib.pyplot as plt
import numpy as np


def read_history(path: str) -> List[dict]:
    rows = []
    if not os.path.exists(path):
        return rows
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            rows.append({k: _num(row.get(k)) for k in row})
    return rows


def read_batch(path: str) -> List[dict]:
    rows = []
    if not os.path.exists(path):
        return rows
    with open(path, newline="") as f:
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


def is_dominated(candidate: Tuple[float, ...], others: List[Tuple[float, ...]]) -> bool:
    for other in others:
        if all(o >= c for o, c in zip(other, candidate)):
            if any(o > c for o, c in zip(other, candidate)):
                return True
    return False


def pareto_frontier(points: List[Tuple[float, ...]]) -> List[Tuple[float, ...]]:
    frontier = []
    for p in points:
        if not is_dominated(p, points):
            frontier.append(p)
    return frontier


def plot_pareto_frontier_2d(
    x: List[float], y: List[float], fx: List[float], fy: List[float],
    xlabel: str, ylabel: str, title: str, out_path: str
) -> None:
    plt.figure(figsize=(8, 6))
    plt.scatter(x, y, alpha=0.6, label="All evaluations", color="#1E64DC")
    if fx:
        sorted_pairs = sorted(zip(fx, fy))
        fxs, fys = zip(*sorted_pairs)
        plt.plot(fxs, fys, "o-", color="#E85D04", linewidth=2, markersize=8, label="Pareto frontier")
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.title(title)
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(out_path, dpi=150)
    plt.close()


def plot_objectives_over_time(history: List[dict], out_path: str) -> None:
    if not history:
        return
    gens = [h.get("generation", i + 1) for i, h in enumerate(history)]
    effectiveness = [h.get("best_effectiveness", 0.0) for h in history]
    deploy_cost = [h.get("best_deploy_cost", 0.0) for h in history]

    fig, ax1 = plt.subplots(figsize=(10, 5))
    color1 = "#1E64DC"
    ax1.plot(gens, effectiveness, color=color1, linewidth=2, label="Effectiveness")
    ax1.set_xlabel("Generation")
    ax1.set_ylabel("Effectiveness (P(det)*P(kill))", color=color1)
    ax1.tick_params(axis="y", labelcolor=color1)
    ax1.set_ylim(0, 1.05)

    ax2 = ax1.twinx()
    color2 = "#E85D04"
    ax2.plot(gens, deploy_cost, color=color2, linewidth=2, linestyle="--", label="Deployment cost")
    ax2.set_ylabel("Deployment cost", color=color2)
    ax2.tick_params(axis="y", labelcolor=color2)

    fig.legend(loc="upper right")
    fig.tight_layout()
    plt.savefig(out_path, dpi=150)
    plt.close()


def main():
    ap = argparse.ArgumentParser(description="GA Pareto frontier analysis")
    ap.add_argument("--history", default=None, help="Path to ga_history.csv")
    ap.add_argument("--batch", default=None, help="Path to ga_batch.csv")
    ap.add_argument("--outdir", default=None, help="Output directory for plots")
    args = ap.parse_args()

    history_path = args.history or os.path.join(os.path.dirname(__file__), "..", "scenarios", "ga_history.csv")
    batch_path = args.batch or os.path.join(os.path.dirname(__file__), "..", "runs", "ga_batch.csv")
    outdir = args.outdir or os.path.dirname(history_path) or "."

    history = read_history(history_path)
    batch = read_batch(batch_path)

    if not history and not batch:
        print("No GA history or batch data found.")
        return

    print(f"Loaded {len(history)} history rows, {len(batch)} batch rows")

    # ── Plot effectiveness vs deployment cost over time ────────────
    if history:
        out1 = os.path.join(outdir, "ga_objectives_over_time.png")
        plot_objectives_over_time(history, out1)
        print(f"Saved {out1}")

    # ── Pareto frontier from batch data ────────────────────────────
    if batch:
        effs = [r.get("effectiveness", 0.0) for r in batch]
        costs = [r.get("total_deployment_cost", 0.0) for r in batch]
        points = list(zip(effs, costs))
        frontier = pareto_frontier(points)
        print(f"Pareto frontier: {len(frontier)} / {len(points)} points")

        out2 = os.path.join(outdir, "ga_pareto_effectiveness_cost.png")
        fx = [p[0] for p in frontier]
        fy = [p[1] for p in frontier]
        plot_pareto_frontier_2d(
            effs, costs, fx, fy,
            "Effectiveness", "Deployment cost",
            "Pareto Frontier: Effectiveness vs Cost",
            out2,
        )
        print(f"Saved {out2}")

        # ── Additional Pareto: P(detected) vs P(killed) ────────────
        p_det = [r.get("probability_detected", 0.0) for r in batch]
        p_kill = [r.get("probability_killed", 0.0) for r in batch]
        points2 = list(zip(p_det, p_kill))
        frontier2 = pareto_frontier(points2)
        print(f"Pareto (Pdet, Pkill): {len(frontier2)} / {len(points2)} points")

        out3 = os.path.join(outdir, "ga_pareto_detection_kill.png")
        fx2 = [p[0] for p in frontier2]
        fy2 = [p[1] for p in frontier2]
        plot_pareto_frontier_2d(
            p_det, p_kill, fx2, fy2,
            "P(detected)", "P(killed)",
            "Pareto Frontier: Detection vs Kill Probability",
            out3,
        )
        print(f"Saved {out3}")


if __name__ == "__main__":
    main()
