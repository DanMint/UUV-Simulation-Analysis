import json
import os
import matplotlib.pyplot as plt
import numpy as np


def plot_runs():

    base_dir = os.path.dirname(os.path.abspath(__file__))

    runs_folder = os.path.join(base_dir, "runs")
    scenario_path = os.path.join(base_dir, "scenario.json")
    output_folder = os.path.join(base_dir, "paths")

    os.makedirs(output_folder, exist_ok=True)

    print(f"Saving plots to: {output_folder}")

    with open(scenario_path, "r") as f:
        scenario = json.load(f)

    grid = np.array(scenario["grid"])

    run_files = [f for f in os.listdir(runs_folder) if f.endswith(".json")]

    if not run_files:
        print("No run files found in runs folder.")
        return

    run_files.sort()

    for filename in run_files:

        run_path = os.path.join(runs_folder, filename)

        try:
            with open(run_path, "r") as f:
                results = json.load(f)

            if "seekers" not in results:
                print(f"Skipping {filename} (no seekers field)")
                continue

        except Exception as e:
            print(f"Error reading {filename}: {e}")
            continue

        fig, ax = plt.subplots(figsize=(10, 10))

        ax.imshow(grid, cmap="Blues_r", origin="upper")

        # Plot seeker paths
        for seeker in results.get("seekers", []):

            path = seeker.get("move_history", [])
            if not path:
                continue

            rows = [p[0] for p in path]
            cols = [p[1] for p in path]

            label = f"Seeker {seeker['id']}"

            if seeker.get("intercepted", False):
                label += " intercepted"
            elif seeker.get("reached_target", False):
                label += " reached"

            ax.plot(cols, rows, linewidth=2, label=label)

            ax.scatter(cols[0], rows[0], marker="o", s=40)
            ax.scatter(cols[-1], rows[-1], marker="x", s=60)

        # Plot targets
        for target in results.get("targets", []):

            r = target["row"]
            c = target["col"]

            if target.get("destroyed", False):
                ax.scatter(c, r, marker="*", s=150, label=f"Target {target['id']} destroyed")
            else:
                ax.scatter(c, r, marker="s", s=80, label=f"Target {target['id']} alive")

        ax.set_title(filename)
        ax.set_xlabel("Column")
        ax.set_ylabel("Row")

        handles, labels = ax.get_legend_handles_labels()
        unique = dict(zip(labels, handles))
        ax.legend(unique.values(), unique.keys(), fontsize=8)

        plt.tight_layout()

        output_file = os.path.join(
            output_folder,
            os.path.splitext(filename)[0] + ".png"
        )

        plt.savefig(output_file, dpi=300)
        plt.close(fig)

        print(f"Saved: {output_file}")


if __name__ == "__main__":
    plot_runs()