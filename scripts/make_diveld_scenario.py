#!/usr/bin/env python3
"""
make_diveld_scenario.py
=======================
Build the reference baseline scenario fixture for the Anduril Dive-LD.

The team planning doc states: "3 Anduril Dive-LD AUVs come from three
different directions."  This script produces a headless scenario with
exactly three `diveld` attackers, each spawned at a distinct cardinal
entry point on the harbour grid, all converging on a single harbor
asset (target).  A single Dive-LD seeker is also added because the
simulation engine requires at least one seeker to drive a run.

It reuses the map metadata + water/land grid from the existing
`scenario.json` so the fixture is guaranteed to sit on valid water cells.

Usage:
    python scripts/make_diveld_scenario.py \
        --out scenarios/diveld_baseline.json \
        [--seed-grid scenario.json]

Output units:
    3x attacker  : vehicle_type="diveld", placed at N / E / S entry points
    1x target    : vehicle_type="diveld", a single harbor asset at center
    1x seeker    : vehicle_type="diveld", near the harbor asset (drives run)
"""

import json
import argparse
import random
import os


WATER = 0          # grid value for a passable water cell (see scenario.json)
LAND  = 1


def find_water_cells(grid):
    """Return a list of (row, col) tuples for all water cells in the grid."""
    cells = []
    for r, row in enumerate(grid):
        for c, val in enumerate(row):
            if val == WATER:
                cells.append((r, c))
    return cells


def pick_entry_from(water, quadrant, rng):
    """
    Pick a water cell near the requested edge of the grid.
    quadrant in {"north","south","east","west"} selects which edge to bias.
    """
    rows = [r for (r, c) in water]
    cols = [c for (r, c) in water]
    min_r, max_r = min(rows), max(rows)
    min_c, max_c = min(cols), max(cols)

    if quadrant == "north":
        candidates = [(r, c) for (r, c) in water if r <= min_r + 4]
    elif quadrant == "south":
        candidates = [(r, c) for (r, c) in water if r >= max_r - 4]
    elif quadrant == "east":
        candidates = [(r, c) for (r, c) in water if c >= max_c - 4]
    elif quadrant == "west":
        candidates = [(r, c) for (r, c) in water if c <= min_c + 4]
    else:
        raise ValueError(f"Unknown quadrant: {quadrant}")

    if not candidates:
        candidates = water  # fall back to any water cell
    return rng.choice(candidates)


def build_scenario(base_path, out_path, seed):
    rng = random.Random(seed)

    with open(base_path, "r", encoding="utf-8") as f:
        base = json.load(f)

    grid = base["grid"]
    water = find_water_cells(grid)

    # Single harbor asset (target) near the centre of the water body.
    rows = [r for (r, c) in water]
    cols = [c for (r, c) in water]
    mid_r = (min(rows) + max(rows)) // 2
    mid_c = (min(cols) + max(cols)) // 2
    target = min(water, key=lambda p: (p[0]-mid_r)**2 + (p[1]-mid_c)**2)

    # Three distinct entry directions.
    nw = pick_entry_from(water, "north", rng)
    ew = pick_entry_from(water, "east", rng)
    sw = pick_entry_from(water, "south", rng)

    attackers = [
        {"type": "attacker", "row": nw[0], "col": nw[1], "vehicle_type": "diveld"},
        {"type": "attacker", "row": ew[0], "col": ew[1], "vehicle_type": "diveld"},
        {"type": "attacker", "row": sw[0], "col": sw[1], "vehicle_type": "diveld"},
    ]
    targets = [
        {"type": "target", "row": target[0], "col": target[1], "vehicle_type": "diveld"},
    ]

    # The simulation engine requires at least one seeker to drive a
    # simulation run. Add a single Dive-LD seeker near the harbour asset
    # so the baseline scenario can execute headless. The 3 Dive-LD
    # attackers remain the focus of the scenario.
    seeker = {"type": "seeker", "row": target[0] + 2, "col": target[1],
              "vehicle_type": "diveld"}

    scenario = {
        "map": base["map"],
        "grid": grid,
        "detector_radius": base.get("detector_radius", 3),
        "interceptor_radius": base.get("interceptor_radius", 3),
        "max_noise_level": base.get("max_noise_level", 0),
        "attacker_zones": base.get("attacker_zones", []),
        "defender_zones": base.get("defender_zones", []),
        "units": attackers + targets + [seeker],
    }

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(scenario, f)

    print(f"[OK] Wrote {out_path}")
    print(f"  target (harbor asset) : row={target[0]:>3} col={target[1]:>3}")
    print(f"  attacker#1 (north)    : row={nw[0]:>3} col={nw[1]:>3}")
    print(f"  attacker#2 (east)     : row={ew[0]:>3} col={ew[1]:>3}")
    print(f"  attacker#3 (south)    : row={sw[0]:>3} col={sw[1]:>3}")
    distinct = {("north", (nw[0], nw[1])), ("east", (ew[0], ew[1])), ("south", (sw[0], sw[1]))}
    assert len(distinct) == 3, "Attackers must start at 3 distinct cells"
    print("  [PASS] 3 distinct entry cells from 3 directions")


def main():
    ap = argparse.ArgumentParser(description="Build the Dive-LD baseline scenario.")
    ap.add_argument("--out", default="scenarios/diveld_baseline.json",
                    help="output scenario JSON path")
    ap.add_argument("--seed-grid", default="scenario.json",
                    help="existing scenario JSON to source map+grid from")
    ap.add_argument("--seed", type=int, default=42,
                    help="RNG seed for reproducibility")
    args = ap.parse_args()
    build_scenario(args.seed_grid, args.out, args.seed)


if __name__ == "__main__":
    main()
