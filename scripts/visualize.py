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

import json
import os
import sys

import matplotlib.pyplot as plt
import numpy as np

# Vehicle-type -> colour (mirrors SimulationVisualizer::attackerColor)
TYPE_COLORS = {
    "bluerov2":    "#FF8C3C",
    "riptide":     "#D2FF3C",
    "blueboat":    "#3CDC82",
    "yuco":        "#00DCC8",
    "nemosens":    "#FF50C8",
    "hugin":       "#8C4614",
    "tb2":         "#00FFFF",
    "queenhornet": "#C8B4FF",
    "shahed":      "#FFBEDC",
}
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
    plot_runs(runs, scen, out)

