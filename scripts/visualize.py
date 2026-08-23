"""
visualize.py
============

Replacement for the root-level (typo-named) `visulaize.py`.

Plots one figure per run JSON in `runs/`:
  - water/land grid background (from scenario.json)
  - seeker paths (red), targets (blue squares), detectors (orange),
    interceptors (purple), attacker paths (color-coded by vehicle type)

Usage:
    python scripts/visualize.py [runs_dir] [scenario.json]

Outputs PNGs into `paths/` next to the runs folder.
"""

import csv
import json
import os
import sys

from matplotlib.colors import ListedColormap
import matplotlib.pyplot as plt
import numpy as np

# Vehicle-type -> colour (mirrors SimulationVisualizer::attackerColor)
TYPE_COLORS = {
    "bluerov2":    "#FF8C3C",
    "riptide":     "#D2FF3C",
    "blueboat":    "#3CDC82",
    "yuco":        "#00DCC8",
    "nemosens":    "#00A8DC",
    "hugin":       "#5050FF",
    "tb2":         "#FF3C3C",
    "queenhornet": "#FF3CFF",
    "shahed":      "#8C3CFF",
    "diveld":      "#FFB400",
}


def _num(v):
    if v is None:
        return 0.0
    try:
        return float(v)
    except (ValueError, TypeError):
        return 0.0
DEFAULT_TYPE_COLOR = "#FF8C3C"


def _type_color(agent_type: str) -> str:
    return TYPE_COLORS.get(agent_type, DEFAULT_TYPE_COLOR)


def plot_runs(runs_dir: str, scenario_path: str, output_dir: str) -> None:
    os.makedirs(output_dir, exist_ok=True)
    print(f"Saving plots to: {output_dir}")

    with open(scenario_path, "r") as f:
        scenario = json.load(f)

    grid = np.array(scenario["grid"])

    run_files = sorted(
        f for f in os.listdir(runs_dir)
        if f.endswith(".json") and f != "summary.json"
    )
    if not run_files:
        print("No run files found in runs folder.")
        return

    for filename in run_files:
        run_path = os.path.join(runs_dir, filename)
        try:
            with open(run_path, "r") as f:
                results = json.load(f)
        except Exception as e:
            print(f"Error reading {filename}: {e}")
            continue

        fig, ax = plt.subplots(figsize=(10, 10))
        ax.imshow(grid, cmap="Blues_r", origin="upper")

        # ── Seekers (red) ─────────────────────────────────────────────
        for seeker in results.get("seekers", []):
            path = seeker.get("move_history", [])
            if not path:
                continue
            rows = [p[0] for p in path]
            cols = [p[1] for p in path]
            label = f"Seeker {seeker['id']}"
            if seeker.get("intercepted", False):
                label += " (intercepted)"
            elif seeker.get("reached_target", False):
                label += " (reached)"
            ax.plot(cols, rows, linewidth=2, color="#DC1E1E", label=label)
            ax.scatter(cols[0], rows[0], marker="o", s=40, color="#DC1E1E")
            ax.scatter(cols[-1], rows[-1], marker="x", s=60, color="#DC1E1E")

        # ── Attackers (color by vehicle type) ─────────────────────────
        for attacker in results.get("attackers", []):
            path = attacker.get("move_history", [])
            if not path:
                continue
            rows = [p[0] for p in path]
            cols = [p[1] for p in path]
            a_type = attacker.get("agent_type", "unknown")
            color = _type_color(a_type)
            label = f"{a_type} {attacker['id']}"
            if attacker.get("mission_success", False):
                label += " (success)"
            ax.plot(cols, rows, linewidth=2, linestyle="--",
                    color=color, label=label)
            ax.scatter(cols[0], rows[0], marker="^", s=60, color=color)
            ax.scatter(cols[-1], rows[-1], marker="*", s=120, color=color)

        # ── Targets (blue squares) ────────────────────────────────────
        for target in results.get("targets", []):
            r, c = target["row"], target["col"]
            if target.get("destroyed", False):
                ax.scatter(c, r, marker="*", s=150,
                           color="#1E64DC", label=f"Target {target['id']} destroyed")
            else:
                ax.scatter(c, r, marker="s", s=80,
                           color="#1E64DC", label=f"Target {target['id']} alive")

        # ── Detectors (orange diamonds) ───────────────────────────────
        for det in results.get("detectors", []):
            ax.scatter(det["col"], det["row"], marker="D", s=90,
                       color="#FFB400", label=f"Detector {det['id']}")

        # ── Interceptors (purple diamonds) ────────────────────────────
        for ic in results.get("interceptors", []):
            ax.scatter(ic["col"], ic["row"], marker="D", s=90,
                       color="#A050FF", label=f"Interceptor {ic['id']}")

        ax.set_title(filename)
        ax.set_xlabel("Column")
        ax.set_ylabel("Row")

        handles, labels = ax.get_legend_handles_labels()
        unique = dict(zip(labels, handles))
        ax.legend(unique.values(), unique.keys(), fontsize=8)

        plt.tight_layout()
        output_file = os.path.join(output_dir,
                                   os.path.splitext(filename)[0] + ".png")
        plt.savefig(output_file, dpi=300)
        plt.close(fig)
        print(f"Saved: {output_file}")


if __name__ == "__main__":
    base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    runs = sys.argv[1] if len(sys.argv) > 1 else os.path.join(base, "runs")
    scen = sys.argv[2] if len(sys.argv) > 2 else os.path.join(base, "scenario.json")
    out = os.path.join(base, "paths")
    if not os.path.isabs(runs):
        runs = os.path.join(base, runs)
    if not os.path.isabs(scen):
        scen = os.path.join(base, scen)
    plot_runs(runs, scen, out)


# ════════════════════════════════════════════════════════════════════════════════
#  GA-specific visualizations
# ════════════════════════════════════════════════════════════════════════════════

def plot_ga_convergence(history_path: str, out_path: str) -> None:
    rows = []
    if not os.path.exists(history_path):
        return
    with open(history_path, newline="") as f:
        for row in csv.DictReader(f):
            rows.append({k: _num(row.get(k)) for k in row})
    if not rows:
        return

    gens = [r.get("generation", i + 1) for i, r in enumerate(rows)]
    best = [r.get("best_fitness", 0.0) for r in rows]
    avg = [r.get("avg_fitness", 0.0) for r in rows]
    diversity = [r.get("diversity", 0.0) for r in rows]

    fig, axes = plt.subplots(2, 1, figsize=(10, 8), sharex=True)

    axes[0].plot(gens, best, label="Best ever", linewidth=2, color="#1E64DC")
    axes[0].plot(gens, avg, label="Population average", linewidth=1, alpha=0.7, color="#888")
    axes[0].fill_between(gens, best, alpha=0.1, color="#1E64DC")
    axes[0].set_ylabel("Fitness")
    axes[0].set_title("GA Convergence")
    axes[0].legend()
    axes[0].grid(True, alpha=0.3)

    axes[1].plot(gens, diversity, linewidth=2, color="#E85D04")
    axes[1].set_ylabel("Diversity")
    axes[1].set_xlabel("Generation")
    axes[1].grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(out_path, dpi=150)
    plt.close()
    print(f"Saved GA convergence: {out_path}")


def plot_ga_scenario(scenario_path: str, out_path: str) -> None:
    if not os.path.exists(scenario_path):
        return
    with open(scenario_path, "r") as f:
        data = json.load(f)

    grid = data.get("grid", [])
    units = data.get("units", [])
    if not grid or not units:
        return

    rows = len(grid)
    cols = len(grid[0])
    arr = np.array(grid)

    fig, ax = plt.subplots(figsize=(10, 10))
    cmap = ListedColormap(["#A8D0E6", "#6B4226"])
    ax.imshow(arr, cmap=cmap, origin="upper")

    for unit in units:
        t = unit.get("type", "")
        r, c = unit.get("row", 0), unit.get("col", 0)
        if t == "target":
            ax.scatter(c, r, marker="s", s=120, color="#1E64DC", label="Target", zorder=5)
        elif t == "seeker":
            ax.scatter(c, r, marker="o", s=80, color="#FF3C3C", label="Seeker", zorder=5)
        elif t == "detector":
            radius = unit.get("sensing_radius", 4)
            circle = plt.Circle((c, r), radius, color="#FFB400", fill=False, linewidth=1.5, linestyle="--")
            ax.add_patch(circle)
            ax.scatter(c, r, marker="D", s=90, color="#FFB400", label="Detector", zorder=5)
        elif t == "interceptor":
            radius = unit.get("kill_radius", 3)
            circle = plt.Circle((c, r), radius, color="#A050FF", fill=False, linewidth=1.5, linestyle="--")
            ax.add_patch(circle)
            ax.scatter(c, r, marker="D", s=90, color="#A050FF", label="Interceptor", zorder=5)
        elif t == "attacker":
            vt = unit.get("vehicle_type", "unknown")
            color = TYPE_COLORS.get(vt, "#888888")
            ax.scatter(c, r, marker="^", s=100, color=color, label=f"Attacker ({vt})", zorder=5)

    handles, labels = ax.get_legend_handles_labels()
    unique = dict(zip(labels, handles))
    ax.legend(unique.values(), unique.keys(), fontsize=8, loc="upper right")
    ax.set_title("GA Best Scenario Layout")
    ax.set_xlabel("Column")
    ax.set_ylabel("Row")
    plt.tight_layout()
    plt.savefig(out_path, dpi=150)
    plt.close()
    print(f"Saved GA scenario: {out_path}")

