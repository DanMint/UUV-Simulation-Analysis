"""
genetic_algorithm.py
====================

A real Genetic Algorithm that wraps the UUV simulator as its fitness function.

Phase 16 of the project: the GA no longer solves a toy knapsack problem —
it now drives `uuv_sim.exe --repeat N` so that each chromosome's fitness is
measured by *actual simulated engagement outcomes*.

Two optimizers are provided:

  1. Defender optimizer  (default)
     Chromosome = placement of detectors + interceptors inside defender zones.
     Fitness   = effectiveness - lambda * (deployment cost / budget)
     effectiveness = P(detected) * P(killed)   (read from runs/ga_batch.csv)

  2. Attacker optimizer  (--side attacker)
     Chromosome = attacker spawn positions + vehicle types inside attacker zones.
     Fitness   = (targets destroyed / total targets) - lambda * (attacker cost / budget)

The simulator is invoked as:
    uuv_sim.exe --scenario <chromo_scenario.json> --repeat N --seed S --no-prompt
and the per-run fitness samples are read from `runs/ga_batch.csv`.

Usage:
    python scripts/genetic_algorithm.py --scenario scenario.json \
        --side defender --pop 40 --gens 30 --repeat 5 --seed 1

Requires: numpy, matplotlib, and a built `uuv_sim.exe` on PATH (or --sim path).
"""

from __future__ import annotations

import argparse
import copy
import csv
import json
import os
import random
import subprocess
import sys

import matplotlib.pyplot as plt
import numpy as np

# ─────────────────────────────────────────────────────────────────────────────
#  Vehicle registry (mirrors src/agents/vehicleSpecs.cpp)
# ─────────────────────────────────────────────────────────────────────────────
VEHICLES = {
    "bluerov2":    {"cost_min": 6000,    "aerial": False, "surface": False},
    "riptide":     {"cost_min": 15000,   "aerial": False, "surface": False},
    "blueboat":    {"cost_min": 5000,    "aerial": False, "surface": True},
    "yuco":        {"cost_min": 50000,   "aerial": False, "surface": False},
    "nemosens":    {"cost_min": 60000,   "aerial": False, "surface": False},
    "hugin":       {"cost_min": 2000000, "aerial": False, "surface": False},
    "tb2":         {"cost_min": 2000000, "aerial": True,  "surface": False},
    "queenhornet": {"cost_min": 1000,    "aerial": True,  "surface": False},
    "shahed":      {"cost_min": 20000,   "aerial": True,  "surface": False},
}

# Detector / interceptor deployment cost (1-3 scale, used by SimResult)
DETECTOR_COST = 1.0
INTERCEPTOR_COST = 2.0


# ─────────────────────────────────────────────────────────────────────────────
#  Scenario I/O helpers
# ─────────────────────────────────────────────────────────────────────────────
def load_scenario(path: str) -> dict:
    with open(path, "r") as f:
        return json.load(f)


def water_cells_in_zone(scenario: dict, zone: dict) -> list[tuple[int, int]]:
    """Return all water cells strictly inside a zone rectangle."""
    grid = scenario.get("grid", [])
    cells = []
    for r in range(zone["rowMin"], zone["rowMax"] + 1):
        for c in range(zone["colMin"], zone["colMax"] + 1):
            if 0 <= r < len(grid) and 0 <= c < len(grid[0]) and grid[r][c] == 0:
                cells.append((r, c))
    return cells


def save_scenario(scenario: dict, path: str) -> None:
    with open(path, "w") as f:
        json.dump(scenario, f, indent=2)


def read_ga_batch(csv_path: str) -> list[dict]:
    """Read runs/ga_batch.csv produced by uuv_sim.exe --repeat N."""
    rows = []
    if not os.path.exists(csv_path):
        return rows
    with open(csv_path, newline="") as f:
        for row in csv.DictReader(f):
            rows.append({
                k: float(v) for k, v in row.items()
            })
    return rows


# ─────────────────────────────────────────────────────────────────────────────
#  Chromosome <-> Scenario mapping
# ─────────────────────────────────────────────────────────────────────────────
def chromo_to_scenario_defender(base: dict, chromo, defender_zones, n_det, n_int):
    """Write detector/interceptor placements into a copy of the scenario.

    Chromosome layout (defender):
      n_det cells chosen for detectors, n_int cells chosen for interceptors.
      Each placement is an index into the flattened union of defender-zone
      water cells.
    """
    sc = copy.deepcopy(base)
    pool = []
    for zone in defender_zones:
        pool.extend(water_cells_in_zone(sc, zone))
    # Deduplicate pool cells across overlapping zones
    pool = list(dict.fromkeys(pool))

    # Strip existing defenders
    sc["units"] = [u for u in sc.get("units", [])
                   if u.get("type") not in ("detector", "interceptor")]

    for i in range(n_det):
        idx = chromo[i] % len(pool)
        r, c = pool[idx]
        sc["units"].append({"type": "detector", "row": r, "col": c})
    for i in range(n_int):
        idx = chromo[n_det + i] % len(pool)
        r, c = pool[idx]
        sc["units"].append({"type": "interceptor", "row": r, "col": c})
    return sc


def chromo_to_scenario_attacker(base: dict, chromo, attacker_zones, n_atk):
    """Write attacker placements into a copy of the scenario.

    Chromosome layout (attacker):
      For each attacker: (cell_index, vehicle_type_index)
    """
    sc = copy.deepcopy(base)
    pool = []
    for zone in attacker_zones:
        pool.extend(water_cells_in_zone(sc, zone))
    pool = list(dict.fromkeys(pool))

    vtypes = list(VEHICLES.keys())
    # Strip existing attackers/seekers
    sc["units"] = [u for u in sc.get("units", [])
                   if u.get("type") not in ("attacker",)]

    for i in range(n_atk):
        cell_idx = chromo[2 * i] % len(pool)
        vt_idx = chromo[2 * i + 1] % len(vtypes)
        r, c = pool[cell_idx]
        sc["units"].append({
            "type": "attacker",
            "row": r,
            "col": c,
            "vehicleType": vtypes[vt_idx],
        })
    return sc


# ─────────────────────────────────────────────────────────────────────────────
#  Fitness estimation (drives the simulator)
# ─────────────────────────────────────────────────────────────────────────────
def evaluate_defender(base, chromo, defender_zones, n_det, n_int,
                      sim_exe, scenario_path, repeat, seed, budget, lam, workdir):
    sc = chromo_to_scenario_defender(base, chromo, defender_zones, n_det, n_int)
    save_scenario(sc, scenario_path)

    csv_path = os.path.join(workdir, "ga_batch.csv")
    if os.path.exists(csv_path):
        os.remove(csv_path)

    cmd = [sim_exe, "--scenario", scenario_path, "--repeat", str(repeat),
           "--seed", str(seed), "--no-prompt"]
    subprocess.run(cmd, cwd=workdir, check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    rows = read_ga_batch(csv_path)
    if not rows:
        return 0.0, 0.0, 0.0

    p_det = np.mean([r["probability_detected"] for r in rows])
    p_kill = np.mean([r["probability_killed"] for r in rows])
    deploy = n_det * DETECTOR_COST + n_int * INTERCEPTOR_COST

    effectiveness = p_det * p_kill
    penalty = lam * (deploy / max(budget, 1e-9))
    fitness = effectiveness - penalty if effectiveness > 0 else 0.0
    return fitness, effectiveness, deploy


def evaluate_attacker(base, chromo, attacker_zones, n_atk,
                      sim_exe, scenario_path, repeat, seed, budget, lam, workdir):
    sc = chromo_to_scenario_attacker(base, chromo, attacker_zones, n_atk)
    save_scenario(sc, scenario_path)

    csv_path = os.path.join(workdir, "ga_batch.csv")
    if os.path.exists(csv_path):
        os.remove(csv_path)

    cmd = [sim_exe, "--scenario", scenario_path, "--repeat", str(repeat),
           "--seed", str(seed), "--no-prompt"]
    subprocess.run(cmd, cwd=workdir, check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    rows = read_ga_batch(csv_path)
    if not rows:
        return 0.0, 0.0

    # Attacker fitness: approximate from ga_batch (which stores P(detected)
    # and P(killed) of the DEFENCE). For attacker optimisation we invert:
    # attackers want targets destroyed & low cost. We read the CSV rows but
    # the key metric for attackers is target destruction — approximated here
    # from (1 - p_kill) as a proxy for "attacker survival".
    p_survive = 1.0 - np.mean([r["probability_killed"] for r in rows])

    cost = 0.0
    vtypes = list(VEHICLES.keys())
    for i in range(n_atk):
        vt_idx = chromo[2 * i + 1] % len(vtypes)
        cost += VEHICLES[vtypes[vt_idx]]["cost_min"]

    penalty = lam * (cost / max(budget, 1e-9))
    fitness = p_survive - penalty if p_survive > 0 else 0.0
    return fitness, cost


# ─────────────────────────────────────────────────────────────────────────────
#  Genetic operators
# ─────────────────────────────────────────────────────────────────────────────
def random_chromo_defender(pool_size, n_det, n_int, rng):
    return [rng.randrange(pool_size) for _ in range(n_det + n_int)]


def random_chromo_attacker(pool_size, n_atk, n_types, rng):
    return [rng.randrange(pool_size) if i % 2 == 0 else rng.randrange(n_types)
            for i in range(2 * n_atk)]


def tournament_select(scores, k, rng):
    """Select one parent index via k-way tournament."""
    best = None
    for _ in range(k):
        idx = rng.randrange(len(scores))
        if best is None or scores[idx] > scores[best]:
            best = idx
    return best


def crossover(a, b, rng):
    pt = rng.randrange(1, len(a))
    return a[:pt] + b[pt:], b[:pt] + a[pt:]


def mutate(chromo, pool_size, n_types, pm, rng):
    out = chromo[:]
    for i in range(len(out)):
        if rng.random() < pm:
            if i % 2 == 0 and pool_size > 0:
                out[i] = rng.randrange(pool_size)
            else:
                out[i] = rng.randrange(n_types)
    return out


# ─────────────────────────────────────────────────────────────────────────────
#  Main GA loop
# ─────────────────────────────────────────────────────────────────────────────
def run_ga(args):
    base = load_scenario(args.scenario)
    rng = random.Random(args.seed)
    workdir = os.path.dirname(os.path.abspath(args.scenario))

    sim_exe = args.sim if args.sim else "uuv_sim.exe"
    scenario_path = os.path.join(workdir, "ga_chromo_scenario.json")

    side = args.side
    pop_size = args.pop
    generations = args.gens
    repeat = args.repeat
    lam = args.lambda_

    if side == "defender":
        zones = base.get("defenderZones", base.get("defender_zones", []))
        if not zones:
            print("Error: no defender zones in scenario. Draw zones with X in the spawn tool.")
            sys.exit(1)
        pool_size = len(set().union(*[set(water_cells_in_zone(base, z)) for z in zones]))
        n_det = args.n_detectors
        n_int = args.n_interceptors
        budget = args.budget
        pop = [random_chromo_defender(pool_size, n_det, n_int, rng)
               for _ in range(pop_size)]
        eval_fn = lambda c: evaluate_defender(
            base, c, zones, n_det, n_int, sim_exe, scenario_path,
            repeat, args.seed, budget, lam, workdir)
    else:
        zones = base.get("attackerZones", base.get("attacker_zones", []))
        if not zones:
            print("Error: no attacker zones in scenario. Draw zones with Z in the spawn tool.")
            sys.exit(1)
        pool_size = len(set().union(*[set(water_cells_in_zone(base, z)) for z in zones]))
        n_atk = args.n_attackers
        budget = args.budget
        pop = [random_chromo_attacker(pool_size, n_atk, len(VEHICLES), rng)
               for _ in range(pop_size)]
        eval_fn = lambda c: evaluate_attacker(
            base, c, zones, n_atk, sim_exe, scenario_path,
            repeat, args.seed, budget, lam, workdir)

    print(f"\n=== UUV GA ({side} optimizer) ===")
    print(f"  Population : {pop_size}")
    print(f"  Generations: {generations}")
    print(f"  Repeat/run : {repeat}")
    print(f"  Seed       : {args.seed}")
    print(f"  Budget     : {budget}  lambda={lam}")
    print(f"  Zone water cells: {pool_size}")
    print("=" * 40)

    # Evaluate initial population
    scores = []
    details = []
    for i, chromo in enumerate(pop):
        res = eval_fn(chromo)
        if side == "defender":
            scores.append(res[0])
            details.append(res)
        else:
            scores.append(res[0])
            details.append(res)
        if (i + 1) % 10 == 0:
            print(f"  Initial eval {i+1}/{pop_size}")

    best_overall = max(scores)
    best_hist = [best_overall]
    avg_hist = [float(np.mean(scores))]

    for gen in range(generations):
        new_pop = []
        while len(new_pop) < pop_size:
            p1 = tournament_select(scores, k=3, rng=rng)
            p2 = tournament_select(scores, k=3, rng=rng)
            c1, c2 = crossover(pop[p1], pop[p2], rng)
            c1 = mutate(c1, pool_size, len(VEHICLES), args.pm, rng)
            c2 = mutate(c2, pool_size, len(VEHICLES), args.pm, rng)
            new_pop.append(c1)
            if len(new_pop) < pop_size:
                new_pop.append(c2)

        # Elitism: keep best of previous generation
        best_idx = max(range(len(scores)), key=lambda i: scores[i])
        new_pop[0] = pop[best_idx]

        pop = new_pop
        scores = []
        details = []
        for chromo in pop:
            res = eval_fn(chromo)
            scores.append(res[0])
            details.append(res)

        gen_best = max(scores)
        best_overall = max(best_overall, gen_best)
        best_hist.append(best_overall)
        avg_hist.append(float(np.mean(scores)))
        print(f"  Gen {gen+1:3d}/{generations}  best={gen_best:.4f}  "
              f"avg={np.mean(scores):.4f}  overall_best={best_overall:.4f}")

    # ── Report best chromosome ───────────────────────────────────────
    best_idx = max(range(len(scores)), key=lambda i: scores[i])
    best_chromo = pop[best_idx]
    print("\n=== BEST CHROMOSOME ===")
    print(f"  Fitness: {scores[best_idx]:.4f}")
    if side == "defender":
        print(f"  P(detected)*P(killed) (effectiveness): {details[best_idx][1]:.4f}")
        print(f"  Deployment cost: {details[best_idx][2]:.2f}")
        sc = chromo_to_scenario_defender(base, best_chromo, zones, n_det, n_int)
    else:
        print(f"  Attacker survival proxy: {details[best_idx][1]:.4f}")
        print(f"  Attacker cost: {details[best_idx][2]:.2f}")
        sc = chromo_to_scenario_attacker(base, best_chromo, zones, n_atk)

    best_scenario_path = os.path.join(workdir, "ga_best_scenario.json")
    save_scenario(sc, best_scenario_path)
    print(f"  Best scenario written to {best_scenario_path}")

    # ── Convergence plot ─────────────────────────────────────────────
    plt.figure(figsize=(8, 5))
    plt.plot(best_hist, label="Best ever", linewidth=2)
    plt.plot(avg_hist, label="Population average", linewidth=1, alpha=0.7)
    plt.xlabel("Generation")
    plt.ylabel("Fitness")
    plt.title(f"UUV GA Convergence ({side} optimizer, seed={args.seed})")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    out_plot = os.path.join(workdir, "ga_convergence.png")
    plt.savefig(out_plot, dpi=150)
    plt.close()
    print(f"  Convergence plot saved to {out_plot}")


# ─────────────────────────────────────────────────────────────────────────────
#  CLI
# ─────────────────────────────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser(description="Genetic algorithm wrapping uuv_sim.exe")
    ap.add_argument("--scenario", required=True, help="Path to scenario.json")
    ap.add_argument("--side", choices=["defender", "attacker"], default="defender")
    ap.add_argument("--pop", type=int, default=40)
    ap.add_argument("--gens", type=int, default=30)
    ap.add_argument("--repeat", type=int, default=5, help="Simulator repeats per eval")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--pm", type=float, default=0.1, help="Mutation probability")
    ap.add_argument("--lambda", dest="lambda_", type=float, default=0.5,
                    help="Cost penalty weight")
    ap.add_argument("--budget", type=float, default=100.0,
                    help="Deployment budget (1-3 scale for defence, $ for attackers)")
    ap.add_argument("--n-detectors", dest="n_detectors", type=int, default=3)
    ap.add_argument("--n-interceptors", dest="n_interceptors", type=int, default=2)
    ap.add_argument("--n-attackers", dest="n_attackers", type=int, default=5)
    ap.add_argument("--sim", default=None, help="Path to uuv_sim.exe")
    args = ap.parse_args()

    run_ga(args)


if __name__ == "__main__":
    main()
