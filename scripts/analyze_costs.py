"""
analyze_costs.py
================

Cost-benefit analysis for the UUV simulation batch results.

Reads `runs/summary.csv` (produced by uuv_sim.exe's --iterations / --noise-step
batch mode via SimResult::saveCSV) and prints a summary plus plots:
  - red_cost vs blue_cost per run (scatter)
  - mission_success_rate vs total_steps (scatter)
  - loss-exchange-ratio histogram (from JSON summary if present)

Usage:
    python scripts/analyze_costs.py [runs_dir]

Requires: numpy, matplotlib, pandas (optional)
"""

import csv
import json
import os
import sys

import matplotlib.pyplot as plt
import numpy as np

CSV_COLUMNS = [
    "run_id",
    "blue_cost",
    "red_cost",
    "loss_exchange_ratio",
    "targets_destroyed",
    "total_targets",
    "critical_asset_reached",
    "total_steps",
    "mission_success_rate",
    "interceptor_engagements",
]


def load_summary(runs_dir: str):
    csv_path = os.path.join(runs_dir, "summary.csv")
    if not os.path.exists(csv_path):
        print(f"No summary.csv found in {runs_dir}. Run batch mode first:\n"
              f"  uuv_sim.exe --scenario scenario.json --iterations 10 --noise-step 0.1")
        return None

    rows = []
    with open(csv_path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append({k: _num(row.get(k)) for k in CSV_COLUMNS})
    return rows


def _num(v):
    if v is None:
        return 0.0
    try:
        return float(v)
    except ValueError:
        return 0.0


def _natural_key(filename):
    """Sort 'run_1.json' numerically.

    Splits out every number (int or float) in the filename so that
    run_10.json sorts AFTER run_2.json —
    alphabetical sorting would put 10 before 2, corrupting run_id
    -> JSON alignment when batch runs skip --noise-step.
    """
    import re
    nums = [float(p) for p in re.findall(r"\d+(?:\.\d+)?", filename)]
    return nums if nums else [0.0]


def enrich_from_json(rows, runs_dir: str):
    """Pull loss_exchange_ratio from per-run JSON summaries.

    Uses natural (numeric) sort of filenames so run_id N maps to the
    Nth batch output. Since the CSV now carries loss_exchange_ratio
    directly (written by SimResult::saveCSV), this is a fallback that
    only fills missing/zero values from the per-run JSON.
    """
    if not os.path.isdir(runs_dir):
        return rows
    candidates = sorted(
        (f for f in os.listdir(runs_dir)
         if f.endswith(".json") and f != "summary.json"),
        key=_natural_key,
    )
    # Build a lookup by the numeric key embedded in the filename (run order)
    for row in rows:
        rid = int(row.get("run_id", -1))
        # The CSV already has it; skip if non-zero.
        if row.get("loss_exchange_ratio", 0.0) not in (0.0, 0):
            continue
        if 0 <= rid < len(candidates):
            try:
                with open(os.path.join(runs_dir, candidates[rid])) as f:
                    data = json.load(f)
                row["loss_exchange_ratio"] = (
                    data.get("summary", {}).get("loss_exchange_ratio", 0.0))
            except Exception:
                row["loss_exchange_ratio"] = 0.0
    return rows


def report(rows, runs_dir: str):
    if not rows:
        return

    blue = np.array([r["blue_cost"] for r in rows])
    red = np.array([r["red_cost"] for r in rows])
    success = np.array([r["mission_success_rate"] for r in rows])
    targets = np.array([r["targets_destroyed"] for r in rows])
    total_t = np.array([r["total_targets"] for r in rows])
    steps = np.array([r["total_steps"] for r in rows])
    ratio = np.array([r.get("loss_exchange_ratio", 0.0) for r in rows])
    eng = np.array([r.get("interceptor_engagements", 0) for r in rows])

    print("\n" + "=" * 56)
    print("         UUV COST-BENEFIT SUMMARY")
    print("=" * 56)
    print(f"Runs analysed            : {len(rows)}")
    print(f"Avg blue cost (engage)   : {blue.mean():>10.2f}  (interceptor shots * cost/shot)")
    print(f"Avg red cost (deployed)  : {red.mean():>10.2f}  (all attackers deployed)")
    print(f"Avg loss exchange ratio  : {ratio.mean():>10.3f}  (<1 defence efficient, >1 attackers trade up)")
    print(f"Avg interceptor shots    : {eng.mean():>10.1f}")
    print(f"Avg mission success rate : {success.mean():>10.1%}")
    print(f"Avg targets destroyed    : {targets.mean():>10.2f} / {total_t.mean():>5.1f}")
    print(f"Avg total steps          : {steps.mean():>10.1f}")
    if total_t.sum() > 0:
        print(f"Target kill rate         : {targets.sum() / total_t.sum():>10.1%}")
    print("=" * 56)

    # ── Plot 1: cost trade-off scatter ────────────────────────────────
    fig, ax = plt.subplots(figsize=(8, 6))
    sc = ax.scatter(blue, red, c=success, cmap="RdYlGn", s=80,
                    edgecolor="k", linewidth=0.5)
    cb = fig.colorbar(sc, ax=ax)
    cb.set_label("Mission success rate")
    ax.set_xlabel("Blue cost (interceptor engagement spend $)")
    ax.set_ylabel("Red cost (attackers deployed $)")
    ax.set_title("Cost Trade-off per Run")
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    fig.savefig(os.path.join(runs_dir, "cost_tradeoff.png"), dpi=150)
    plt.close(fig)
    print(f"Saved: {os.path.join(runs_dir, 'cost_tradeoff.png')}")

    # ── Plot 2: success vs speed ──────────────────────────────────────
    fig, ax = plt.subplots(figsize=(8, 6))
    ax.scatter(steps, success, s=80, color="#1E64DC",
               edgecolor="k", linewidth=0.5)
    ax.set_xlabel("Total steps (sim length)")
    ax.set_ylabel("Mission success rate")
    ax.set_title("Mission Success vs Sim Length")
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    fig.savefig(os.path.join(runs_dir, "success_vs_steps.png"), dpi=150)
    plt.close(fig)
    print(f"Saved: {os.path.join(runs_dir, 'success_vs_steps.png')}")

# ── Plot 3: loss exchange ratio histogram ─────────────────────────
    if len(ratio) > 1 and ratio.std() > 0:
        fig, ax = plt.subplots(figsize=(8, 6))
        ax.hist(ratio, bins=min(20, len(ratio)), color="#A050FF",
                edgecolor="k", alpha=0.8)
        ax.axvline(1.0, color="k", linestyle="--", label="Breakeven (1.0)")
        ax.set_xlabel("Loss exchange ratio (red/blue)")
        ax.set_ylabel("Runs")
        ax.set_title("Loss Exchange Ratio Distribution")
        ax.legend()
        plt.tight_layout()
        fig.savefig(os.path.join(runs_dir, "loss_exchange_ratio.png"), dpi=150)
        plt.close(fig)
        print(f"Saved: {os.path.join(runs_dir, 'loss_exchange_ratio.png')}")

    # ── Plot 4: break-even line — red_cost vs blue_cost with y=x line ──
    # The break-even line shows where attackers' deployed cost equals the
    # defenders' engagement spend. Points ABOVE the line mean the attackers
    # lost more in hardware than the defence spent shooting — a "good trade"
    # for the defence. Points BELOW the line mean the defence overspent.
    fig, ax = plt.subplots(figsize=(8, 6))
    ax.scatter(blue, red, s=80, c="steelblue", edgecolor="k", linewidth=0.5)
    lo = min(blue.min(), red.min(), 1.0)
    hi = max(blue.max(), red.max(), 1.0)
    ax.plot([lo, hi], [lo, hi], "k--", label="Break-even (red = blue)")
    ax.fill_between([lo, hi], [lo, hi], [hi, hi], color="green", alpha=0.08)
    ax.fill_between([lo, hi], [lo, lo], [lo, hi], color="red", alpha=0.08)
    ax.set_xlabel("Blue cost (interceptor engagement spend $)")
    ax.set_ylabel("Red cost (attackers deployed $)")
    ax.set_title("Break-even Analysis: Did each red exchange cost more than blue spent?")
    ax.legend()
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    fig.savefig(os.path.join(runs_dir, "break_even.png"), dpi=150)
    plt.close(fig)
    print(f"Saved: {os.path.join(runs_dir, 'break_even.png')}")

    # ── Plot 5: interceptor engagement efficiency ─────────────────────
    # cost per intercept = blue_cost / (# interceptors that scored a kill).
    # This shows how expensive each successful kill is — the tension between
    # a cheap BlueROV2 interceptor and an expensive HUGIN one.
    if (eng.sum() > 0):
        efficient = [r["blue_cost"] / max(r["interceptor_engagements"], 1)
                     for r in rows]
        fig, ax = plt.subplots(figsize=(8, 6))
        ax.scatter(eng, efficient, s=80, color="#E85D04",
                   edgecolor="k", linewidth=0.5)
        ax.set_xlabel("Total interceptor shots fired")
        ax.set_ylabel("Cost per shot ($)")
        ax.set_title("Engagement Efficiency: cost per shot vs shots fired")
        ax.grid(True, alpha=0.3)
        plt.tight_layout()
        fig.savefig(os.path.join(runs_dir, "engagement_efficiency.png"), dpi=150)
        plt.close(fig)
        print(f"Saved: {os.path.join(runs_dir, 'engagement_efficiency.png')}")

    # ── Cost-definition explainer ─────────────────────────────────────
    print("\n" + "=" * 56)
    print("       COST-BENEFIT DEFINITIONS (Lance's formula)")
    print("=" * 56)
    print("blue_cost = interceptor ENGAGEMENT spend")
    print("          = sum(shots fired * cost per shot)  [hit or miss]")
    print("  -> captures the '$2M interceptor vs $1000 drone' trade:")
    print("     a HUGIN interceptor's shots are far more expensive than a")
    print("     BlueROV2's.")
    print()
    print("red_cost  = total unit cost of ALL attackers DEPLOYED")
    print("          = sum(unitCostMin of every attacker committed)")
    print("  -> cost is sunk once a unit is committed, whether it succeeds")
    print("     or fails.")
    print()
    print("loss_exchange_ratio = red_cost / blue_cost")
    print("  < 1.0  defence efficient (spent less than it destroyed)")
    print("  = 1.0  break-even")
    print("  > 1.0  attackers trade up (their loss exceeded your spend)")
    print("=" * 56)


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] in ("--help", "-h"):
        print(__doc__.strip())
        sys.exit(0)
    base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    runs_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(base, "runs")
    rows = load_summary(runs_dir)
    if rows is None:
        sys.exit(1)
    rows = enrich_from_json(rows, runs_dir)
    report(rows, runs_dir)

