"""
genetic_algorithm.py
====================

A real Genetic Algorithm that wraps the UUV simulator as its fitness function.

Phase 16/17 of the project: the GA no longer solves a toy knapsack problem —
it now drives `uuv_sim.exe --repeat N` so that each chromosome's fitness is
measured by *actual simulated engagement outcomes*.

Two optimizers are provided:

  1. Defender optimizer  (default)
     Chromosome = placement of detectors + interceptors inside defender zones.
     Fitness   = effectiveness - lambda * (deployment cost / budget)
     Effectiveness = P(detected) * P(killed)

  2. Attacker optimizer
     Chromosome = placement + vehicle type of attackers inside attacker zones.
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
import time
import hashlib
from concurrent.futures import ProcessPoolExecutor, as_completed
from dataclasses import dataclass, field, asdict
from typing import List, Tuple, Optional

import matplotlib.pyplot as plt
import numpy as np

# ─────────────────────────────────────────────────────────────────────────────
#  Vehicle registry (mirrors src/agents/vehicleSpecs.cpp)
# ─────────────────────────────────────────────────────────────────────────────
VEHICLES = {
    "bluerov2":    {"cost_min": 6000,    "aerial": False, "surface": False, "speed": "1-3 kn"},
    "riptide":     {"cost_min": 15000,   "aerial": False, "surface": False, "speed": "2-5 kn"},
    "blueboat":    {"cost_min": 5000,    "aerial": False, "surface": True,  "speed": "2-6 kn"},
    "yuco":        {"cost_min": 50000,   "aerial": False, "surface": False, "speed": "2-6 kn"},
    "nemosens":    {"cost_min": 60000,   "aerial": False, "surface": False, "speed": "2-4 kn"},
    "hugin":       {"cost_min": 2000000, "aerial": False, "surface": False, "speed": "2-5 kn"},
    "tb2":         {"cost_min": 2000000, "aerial": True,  "surface": False, "speed": "90-110 kn"},
    "queenhornet": {"cost_min": 1000,    "aerial": True,  "surface": False, "speed": "38-43 kn"},
    "shahed":      {"cost_min": 20000,   "aerial": True,  "surface": False, "speed": "90-100 kn"},
    "diveld":      {"cost_min": 500000,  "aerial": False, "surface": False, "speed": "1-3 kn"},
}

VEHICLE_KEYS = list(VEHICLES.keys())

# ─────────────────────────────────────────────────────────────────────────────
#  Cost model (must mirror C++ simulator exactly)
# ─────────────────────────────────────────────────────────────────────────────
DETECTOR_UNIT_COST = 1.0
INTERCEPTOR_UNIT_COST = 1.0
INTERCEPTOR_ENGAGEMENT_COST = 250000.0


# ─────────────────────────────────────────────────────────────────────────────
#  Data structures
# ─────────────────────────────────────────────────────────────────────────────

@dataclass
class EvalResult:
    fitness: float
    effectiveness: float = 0.0
    deploy_cost: float = 0.0
    p_detected: float = 0.0
    p_killed: float = 0.0
    targets_destroyed: int = 0
    total_targets: int = 0
    destroy_rate: float = 0.0
    red_cost: float = 0.0
    blue_cost: float = 0.0
    loss_exchange_ratio: float = 0.0
    mission_success_rate: float = 0.0
    interceptor_engagements: int = 0
    chromosome_hash: str = ""
    sim_time: float = 0.0
    error: Optional[str] = None


@dataclass
class GAGeneration:
    generation: int
    best_fitness: float
    avg_fitness: float
    worst_fitness: float
    best_effectiveness: float
    best_deploy_cost: float
    diversity: float
    timestamp: float = field(default_factory=time.time)


# ─────────────────────────────────────────────────────────────────────────────
#  Scenario I/O helpers
# ─────────────────────────────────────────────────────────────────────────────

def load_scenario(path: str) -> dict:
    with open(path, "r") as f:
        return json.load(f)


def water_cells_in_zone(scenario: dict, zone: dict) -> List[Tuple[int, int]]:
    grid = scenario.get("grid", [])
    cells = []
    for r in range(zone["row_min"], zone["row_max"] + 1):
        for c in range(zone["col_min"], zone["col_max"] + 1):
            if 0 <= r < len(grid) and 0 <= c < len(grid[0]) and grid[r][c] == 0:
                cells.append((r, c))
    return cells


def save_scenario(scenario: dict, path: str) -> None:
    with open(path, "w") as f:
        json.dump(scenario, f, indent=2)


def read_ga_batch(csv_path: str) -> List[dict]:
    rows = []
    if not os.path.exists(csv_path):
        return rows
    with open(csv_path, newline="") as f:
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


# ─────────────────────────────────────────────────────────────────────────────
#  Chromosome <-> Scenario mapping
# ─────────────────────────────────────────────────────────────────────────────

def chromo_to_scenario_defender(base: dict, chromo, defender_zones, n_det, n_int):
    sc = copy.deepcopy(base)
    pool = []
    for zone in defender_zones:
        pool.extend(water_cells_in_zone(sc, zone))
    pool = list(dict.fromkeys(pool))

    sc["units"] = [u for u in sc.get("units", [])
                   if u.get("type") not in ("detector", "interceptor")]

    if not pool:
        raise ValueError("Defender zone has no water cells to place defenders")

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
    sc = copy.deepcopy(base)
    pool = []
    for zone in attacker_zones:
        pool.extend(water_cells_in_zone(sc, zone))
    pool = list(dict.fromkeys(pool))

    vtypes = VEHICLE_KEYS
    sc["units"] = [u for u in sc.get("units", [])
                   if u.get("type") not in ("attacker",)]

    if not pool:
        raise ValueError("Attacker zone has no water cells to place attackers")

    for i in range(n_atk):
        cell_idx = chromo[2 * i] % len(pool)
        vt_idx = chromo[2 * i + 1] % len(vtypes)
        r, c = pool[cell_idx]
        sc["units"].append({
            "type": "attacker",
            "row": r,
            "col": c,
            "vehicle_type": vtypes[vt_idx],
        })
    return sc


# ─────────────────────────────────────────────────────────────────────────────
#  Fitness evaluation (drives the simulator)
# ─────────────────────────────────────────────────────────────────────────────

def _run_simulator(sim_exe: str, scenario_path: str, repeat: int, seed: int, workdir: str) -> List[dict]:
    runs_dir = os.path.join(workdir, "runs")
    os.makedirs(runs_dir, exist_ok=True)
    csv_path = os.path.join(runs_dir, "ga_batch.csv")
    if os.path.exists(csv_path):
        os.remove(csv_path)

    cmd = [sim_exe, "--scenario", scenario_path, "--repeat", str(repeat),
           "--seed", str(seed), "--no-prompt"]

    start = time.time()
    try:
        result = subprocess.run(cmd, cwd=workdir, check=True,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               timeout=300)
    except subprocess.TimeoutExpired:
        raise RuntimeError(f"Simulator timed out after 300s: {' '.join(cmd)}")
    except subprocess.CalledProcessError as e:
        raise RuntimeError(f"Simulator exited with code {e.returncode}: {' '.join(cmd)}")
    sim_time = time.time() - start

    if not os.path.exists(csv_path):
        raise RuntimeError(f"Simulator produced no GA batch output: {csv_path}")

    rows = read_ga_batch(csv_path)
    if not rows:
        raise RuntimeError(f"Simulator produced no GA batch output: {csv_path}")

    for r in rows:
        r["sim_time"] = sim_time
    return rows


def evaluate_defender(base, chromo, defender_zones, n_det, n_int,
                      sim_exe, scenario_path, repeat, seed, budget, lam, workdir):
    sc = chromo_to_scenario_defender(base, chromo, defender_zones, n_det, n_int)

    # Ensure a baseline threat package exists so detectors/interceptors have
    # attackers to detect/kill. We add a small fixed set of cheap attackers.
    existing_attackers = [u for u in sc.get("units", []) if u.get("type") == "attacker"]
    if not existing_attackers:
        attacker_zones = base.get("attackerZones", base.get("attacker_zones", []))
        pool = []
        for zone in attacker_zones:
            pool.extend(water_cells_in_zone(sc, zone))
        pool = list(dict.fromkeys(pool))
        rng = random.Random(seed)
        for i in range(min(3, len(pool))):
            idx = rng.randrange(len(pool))
            r, c = pool[idx]
            sc["units"].append({"type": "attacker", "row": r, "col": c,
                                "vehicle_type": "diveld"})

    save_scenario(sc, scenario_path)

    try:
        rows = _run_simulator(sim_exe, scenario_path, repeat, seed, workdir)
    except RuntimeError as e:
        return EvalResult(fitness=0.0, effectiveness=0.0, deploy_cost=float(n_det + n_int),
                          error=str(e))
    return _compute_defender_result(rows, n_det, n_int, budget, lam)


def evaluate_attacker(base, chromo, attacker_zones, n_atk,
                      sim_exe, scenario_path, repeat, seed, budget, lam, workdir):
    sc = chromo_to_scenario_attacker(base, chromo, attacker_zones, n_atk)
    save_scenario(sc, scenario_path)

    try:
        rows = _run_simulator(sim_exe, scenario_path, repeat, seed, workdir)
    except RuntimeError as e:
        return EvalResult(fitness=0.0, destroy_rate=0.0, red_cost=float(n_atk) * 50000.0,
                          error=str(e))
    return _compute_attacker_result(rows, budget, lam)


def _compute_defender_result(rows, n_det, n_int, budget, lam):
    p_det = np.mean([r.get("probability_detected", 0.0) for r in rows])
    p_kill = np.mean([r.get("probability_killed", 0.0) for r in rows])
    avg_blue = np.mean([r.get("blue_cost", 0.0) for r in rows])
    avg_red = np.mean([r.get("red_cost", 0.0) for r in rows])
    ler = np.mean([r.get("loss_exchange_ratio", 0.0) for r in rows])
    t_destroyed = np.sum([r.get("targets_destroyed", 0) for r in rows])
    t_total = np.sum([r.get("total_targets", 0) for r in rows])
    mission_success = np.mean([r.get("mission_success_rate", 0.0) for r in rows])
    eng = np.mean([r.get("interceptor_engagements", 0.0) for r in rows])
    sim_time = np.mean([r.get("sim_time", 0.0) for r in rows])

    effectiveness = p_det * p_kill
    deploy_cost = n_det * DETECTOR_UNIT_COST + n_int * INTERCEPTOR_UNIT_COST
    penalty = lam * (deploy_cost / max(budget, 1e-9))
    fitness = effectiveness - penalty if effectiveness > 0 else 0.0

    return EvalResult(
        fitness=fitness,
        effectiveness=effectiveness,
        deploy_cost=deploy_cost,
        p_detected=p_det,
        p_killed=p_kill,
        targets_destroyed=int(t_destroyed),
        total_targets=int(t_total),
        destroy_rate=t_destroyed / max(t_total, 1e-9),
        blue_cost=avg_blue,
        red_cost=avg_red,
        loss_exchange_ratio=ler,
        mission_success_rate=mission_success,
        interceptor_engagements=int(eng),
        sim_time=sim_time,
    )


def _compute_attacker_result(rows, budget, lam):
    t_destroyed = np.sum([r.get("targets_destroyed", 0) for r in rows])
    t_total = np.sum([r.get("total_targets", 0) for r in rows])
    destroy_rate = t_destroyed / max(t_total, 1e-9)

    red_cost = float(np.mean([r.get("red_cost", 0.0) for r in rows]))
    blue_cost = float(np.mean([r.get("blue_cost", 0.0) for r in rows]))
    ler = float(np.mean([r.get("loss_exchange_ratio", 0.0) for r in rows]))
    mission_success = float(np.mean([r.get("mission_success_rate", 0.0) for r in rows]))
    sim_time = float(np.mean([r.get("sim_time", 0.0) for r in rows]))

    penalty = lam * (red_cost / max(budget, 1e-9))
    fitness = destroy_rate - penalty if destroy_rate > 0 else 0.0

    return EvalResult(
        fitness=fitness,
        destroy_rate=destroy_rate,
        red_cost=red_cost,
        blue_cost=blue_cost,
        loss_exchange_ratio=ler,
        mission_success_rate=mission_success,
        targets_destroyed=int(t_destroyed),
        total_targets=int(t_total),
        sim_time=sim_time,
    )


# ─────────────────────────────────────────────────────────────────────────────
#  Genetic operators
# ─────────────────────────────────────────────────────────────────────────────

def chromo_hash(chromo: List[int]) -> str:
    return hashlib.md5(str(chromo).encode()).hexdigest()[:12]


def random_chromo_defender(pool_size: int, n_det: int, n_int: int, rng: random.Random) -> List[int]:
    return [rng.randrange(pool_size) for _ in range(n_det + n_int)]


def random_chromo_attacker(pool_size: int, n_atk: int, n_types: int, rng: random.Random) -> List[int]:
    return [rng.randrange(pool_size) if i % 2 == 0 else rng.randrange(n_types)
            for i in range(2 * n_atk)]


def tournament_select(scores: List[float], k: int, rng: random.Random) -> int:
    best = None
    for _ in range(k):
        idx = rng.randrange(len(scores))
        if best is None or scores[idx] > scores[best]:
            best = idx
    return best


def crossover(a: List[int], b: List[int], rng: random.Random) -> Tuple[List[int], List[int]]:
    if len(a) != len(b):
        return a[:], b[:]
    pt = rng.randrange(1, len(a))
    return a[:pt] + b[pt:], b[:pt] + a[pt:]


def mutate(chromo: List[int], pool_size: int, n_types: int, pm: float, rng: random.Random) -> List[int]:
    out = chromo[:]
    for i in range(len(out)):
        if rng.random() < pm:
            if i % 2 == 0 and pool_size > 0:
                out[i] = rng.randrange(pool_size)
            else:
                out[i] = rng.randrange(n_types)
    return out


# ─────────────────────────────────────────────────────────────────────────────
#  Diversity metrics
# ─────────────────────────────────────────────────────────────────────────────

def population_diversity(pop: List[List[int]]) -> float:
    unique = len(set(tuple(c) for c in pop))
    return unique / max(len(pop), 1)


def hamming_distance(a: List[int], b: List[int]) -> int:
    return sum(x != y for x, y in zip(a, b))


# ─────────────────────────────────────────────────────────────────────────────
#  Island model (parallel sub-populations)
# ─────────────────────────────────────────────────────────────────────────────

class Island:
    def __init__(self, pop: List[List[int]], scores: List[float], rng: random.Random):
        self.pop = pop
        self.scores = scores
        self.rng = rng

    def evolve(self, pool_size: int, n_types: int, pm: float, k: int = 3) -> None:
        new_pop = []
        while len(new_pop) < len(self.pop):
            p1 = tournament_select(self.scores, k, self.rng)
            p2 = tournament_select(self.scores, k, self.rng)
            c1, c2 = crossover(self.pop[p1], self.pop[p2], self.rng)
            c1 = mutate(c1, pool_size, n_types, pm, self.rng)
            c2 = mutate(c2, pool_size, n_types, pm, self.rng)
            new_pop.append(c1)
            if len(new_pop) < len(self.pop):
                new_pop.append(c2)

        best_idx = max(range(len(self.scores)), key=lambda i: self.scores[i])
        new_pop[0] = self.pop[best_idx]
        self.pop = new_pop
        self.scores = [0.0] * len(self.pop)


def migrate(islands: List[Island], rate: float = 0.1) -> None:
    n = len(islands)
    for i in range(n):
        j = (i + 1) % n
        n_migrate = max(1, int(rate * len(islands[i].pop)))
        migrants = sorted(range(len(islands[i].pop)),
                          key=lambda idx: islands[i].scores[idx],
                          reverse=True)[:n_migrate]
        for idx in migrants:
            islands[j].pop.append(islands[i].pop[idx])
            islands[j].scores.append(islands[i].scores[idx])
        islands[j].pop = islands[j].pop[:len(islands[j].pop) - n_migrate]
        islands[j].scores = islands[j].scores[:len(islands[j].scores) - n_migrate]


# ─────────────────────────────────────────────────────────────────────────────
#  Parallel evaluation helper
# ─────────────────────────────────────────────────────────────────────────────

def _parallel_eval(chromos: List[List[int]], eval_fn, n_jobs: int) -> List[EvalResult]:
    if n_jobs <= 1 or len(chromos) <= 1:
        return [eval_fn(c) for c in chromos]

    results = []
    with ProcessPoolExecutor(max_workers=n_jobs) as executor:
        futures = {executor.submit(eval_fn, c): i for i, c in enumerate(chromos)}
        for future in as_completed(futures):
            idx = futures[future]
            try:
                results.append((idx, future.result()))
            except Exception as e:
                results.append((idx, EvalResult(fitness=0.0, error=str(e))))

    results.sort(key=lambda x: x[0])
    return [r for _, r in results]


# ─────────────────────────────────────────────────────────────────────────────
#  Main GA loop
# ─────────────────────────────────────────────────────────────────────────────

def run_ga(args):
    base = load_scenario(args.scenario)
    rng = random.Random(args.seed)
    workdir = os.path.dirname(os.path.abspath(args.scenario))

    sim_exe = os.path.abspath(args.sim) if args.sim else os.path.abspath("uuv_sim.exe")
    scenario_path = os.path.join(workdir, "ga_chromo_scenario.json")

    side = args.side
    pop_size = args.pop
    generations = args.gens
    repeat = args.repeat
    lam = args.lambda_
    n_islands = args.islands
    pm = args.pm

    if side == "defender":
        zones = base.get("defenderZones", base.get("defender_zones", []))
        if not zones:
            print("Error: no defender zones in scenario. Draw zones with X in the spawn tool.")
            sys.exit(1)
        pool_size = len(set().union(*[set(water_cells_in_zone(base, z)) for z in zones]))
        n_det = args.n_detectors
        n_int = args.n_interceptors
        budget = args.budget if args.budget is not None else 100.0
        chromo_len = n_det + n_int
        random_chromo = lambda r: random_chromo_defender(pool_size, n_det, n_int, r)
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
        budget = args.budget if args.budget is not None else 2000000.0
        chromo_len = 2 * n_atk
        random_chromo = lambda r: random_chromo_attacker(pool_size, n_atk, len(VEHICLES), r)
        eval_fn = lambda c: evaluate_attacker(
            base, c, zones, n_atk, sim_exe, scenario_path,
            repeat, args.seed, budget, lam, workdir)

    print(f"\n=== UUV GA ({side} optimizer) ===")
    print(f"  Population : {pop_size}")
    print(f"  Generations: {generations}")
    print(f"  Islands    : {n_islands}")
    print(f"  Repeat/run : {repeat}")
    print(f"  Seed       : {args.seed}")
    print(f"  Budget     : {budget}  lambda={lam}")
    print(f"  Mutation   : {pm}")
    print(f"  Zone water cells: {pool_size}")
    print("=" * 40)

    # ── Initialize islands ──────────────────────────────────────────
    islands = []
    for i in range(n_islands):
        island_rng = random.Random(args.seed + i * 1000)
        pop = [random_chromo(island_rng) for _ in range(pop_size // n_islands)]
        islands.append(Island(pop, [0.0] * len(pop), island_rng))

    all_hist: List[GAGeneration] = []
    checkpoint_file = os.path.join(workdir, "ga_checkpoint.json")

    # ── Resume from checkpoint if requested ─────────────────────────
    start_gen = 0
    if args.resume and os.path.exists(checkpoint_file):
        print(f"Resuming from {checkpoint_file}")
        with open(checkpoint_file, "r") as f:
            ckpt = json.load(f)
        start_gen = ckpt.get("generation", 0)
        for i, island_data in enumerate(ckpt.get("islands", [])):
            if i < len(islands):
                islands[i].pop = island_data["pop"]
                islands[i].scores = island_data["scores"]
        if "history" in ckpt:
            for h in ckpt["history"]:
                all_hist.append(GAGeneration(**h))
        print(f"  Resumed at generation {start_gen}")

    # ── Evaluate initial populations ─────────────────────────────────
    print("Evaluating initial populations...")
    for island in islands:
        eval_results = _parallel_eval(island.pop, eval_fn, args.jobs)
        island.scores = [r.fitness for r in eval_results]

    best_overall = max(max(island.scores) for island in islands)
    best_hist = [best_overall]
    avg_hist = [float(np.mean([s for island in islands for s in island.scores]))]

    # ── Main GA loop ─────────────────────────────────────────────────
    for gen in range(start_gen, generations):
        gen_start = time.time()

        for island in islands:
            island.evolve(pool_size, len(VEHICLES), pm)

        # Parallel evaluation of all islands
        all_chromos = []
        all_island_idx = []
        for i, island in enumerate(islands):
            all_chromos.extend(island.pop)
            all_island_idx.extend([i] * len(island.pop))

        eval_results = _parallel_eval(all_chromos, eval_fn, args.jobs)

        for idx, result in enumerate(eval_results):
            i = all_island_idx[idx]
            islands[i].scores[idx % len(islands[i].scores)] = result.fitness

        # Migration
        if gen % args.migration_interval == 0 and n_islands > 1:
            migrate(islands, rate=args.migration_rate)

        gen_best = max(max(island.scores) for island in islands)
        best_overall = max(best_overall, gen_best)
        best_hist.append(best_overall)
        avg_hist.append(float(np.mean([s for island in islands for s in island.scores])))
        diversity = float(np.mean([population_diversity(island.pop) for island in islands]))

        all_hist.append(GAGeneration(
            generation=gen + 1,
            best_fitness=gen_best,
            avg_fitness=avg_hist[-1],
            worst_fitness=min(min(island.scores) for island in islands),
            best_effectiveness=max(r.effectiveness for r in eval_results),
            best_deploy_cost=min(r.deploy_cost for r in eval_results),
            diversity=diversity,
        ))

        print(f"  Gen {gen+1:3d}/{generations}  best={gen_best:.4f}  "
              f"avg={avg_hist[-1]:.4f}  overall_best={best_overall:.4f}  "
              f"diversity={diversity:.2%}  time={time.time()-gen_start:.1f}s")

        # ── Checkpoint ───────────────────────────────────────────────
        if args.checkpoint and (gen + 1) % args.checkpoint_interval == 0:
            ckpt = {
                "generation": gen + 1,
                "islands": [
                    {"pop": island.pop, "scores": island.scores}
                    for island in islands
                ],
                "history": [asdict(h) for h in all_hist],
            }
            with open(checkpoint_file, "w") as f:
                json.dump(ckpt, f, indent=2)
            print(f"  [checkpoint saved at gen {gen+1}]")

        # ── Early stopping ───────────────────────────────────────────
        if gen > args.early_stop_patience:
            recent = [h.best_fitness for h in all_hist[-args.early_stop_patience:]]
            if max(recent) - min(recent) < args.early_stop_threshold:
                print(f"\n  Early stopping: no improvement in {args.early_stop_patience} generations")
                break

    # ── Report best chromosome ───────────────────────────────────────
    best_island = max(islands, key=lambda isl: max(isl.scores))
    best_idx = max(range(len(best_island.scores)), key=lambda i: best_island.scores[i])
    best_chromo = best_island.pop[best_idx]
    best_result = evaluate_defender(base, best_chromo, zones, n_det, n_int,
                                    sim_exe, scenario_path, repeat, args.seed, budget, lam, workdir) \
                  if side == "defender" else \
                  evaluate_attacker(base, best_chromo, zones, n_atk,
                                    sim_exe, scenario_path, repeat, args.seed, budget, lam, workdir)

    print("\n=== BEST CHROMOSOME ===")
    print(f"  Fitness: {best_result.fitness:.4f}")
    if side == "defender":
        print(f"  P(detected)*P(killed): {best_result.effectiveness:.4f}")
        print(f"  Deployment cost: {best_result.deploy_cost:.2f}")
        sc = chromo_to_scenario_defender(base, best_chromo, zones, n_det, n_int)
    else:
        print(f"  Target destruction rate: {best_result.destroy_rate:.4f}")
        print(f"  Attacker cost (red): {best_result.red_cost:.2f}")
        sc = chromo_to_scenario_attacker(base, best_chromo, zones, n_atk)

    best_scenario_path = os.path.join(workdir, "ga_best_scenario.json")
    save_scenario(sc, best_scenario_path)
    print(f"  Best scenario written to {best_scenario_path}")

    # ── Convergence plot ─────────────────────────────────────────────
    plt.figure(figsize=(10, 6))
    plt.plot(best_hist, label="Best ever", linewidth=2, color="#1E64DC")
    plt.plot(avg_hist, label="Population average", linewidth=1, alpha=0.7, color="#888")
    plt.fill_between(range(len(best_hist)), best_hist, alpha=0.1, color="#1E64DC")
    plt.xlabel("Generation")
    plt.ylabel("Fitness")
    plt.title(f"UUV GA Convergence ({side} optimizer, seed={args.seed}, islands={n_islands})")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    out_plot = os.path.join(workdir, "ga_convergence.png")
    plt.savefig(out_plot, dpi=150)
    plt.close()
    print(f"  Convergence plot saved to {out_plot}")

    # ── Diversity plot ───────────────────────────────────────────────
    if len(all_hist) > 1:
        plt.figure(figsize=(10, 4))
        plt.plot([h.diversity for h in all_hist], linewidth=2, color="#E85D04")
        plt.xlabel("Generation")
        plt.ylabel("Diversity (unique / pop size)")
        plt.title("Population Diversity Over Time")
        plt.grid(True, alpha=0.3)
        plt.tight_layout()
        div_plot = os.path.join(workdir, "ga_diversity.png")
        plt.savefig(div_plot, dpi=150)
        plt.close()
        print(f"  Diversity plot saved to {div_plot}")

    # ── Save history as CSV ──────────────────────────────────────────
    csv_hist = os.path.join(workdir, "ga_history.csv")
    with open(csv_hist, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=[
            "generation", "best_fitness", "avg_fitness", "worst_fitness",
            "best_effectiveness", "best_deploy_cost", "diversity", "timestamp"
        ])
        writer.writeheader()
        for h in all_hist:
            writer.writerow(asdict(h))
    print(f"  History saved to {csv_hist}")


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
    ap.add_argument("--pm", type=float, default=0.15, help="Mutation probability")
    ap.add_argument("--lambda", dest="lambda_", type=float, default=0.5,
                    help="Cost penalty weight")
    ap.add_argument("--budget", type=float, default=None,
                    help="Deployment budget (defender: unit count scale; attacker: dollar scale; auto-detected if omitted)")
    ap.add_argument("--n-detectors", dest="n_detectors", type=int, default=3)
    ap.add_argument("--n-interceptors", dest="n_interceptors", type=int, default=2)
    ap.add_argument("--n-attackers", dest="n_attackers", type=int, default=5)
    ap.add_argument("--sim", default=None, help="Path to uuv_sim.exe")
    ap.add_argument("--jobs", type=int, default=1,
                    help="Parallel evaluation jobs (1=sequential)")
    ap.add_argument("--islands", type=int, default=1,
                    help="Number of island sub-populations")
    ap.add_argument("--migration-interval", type=int, default=5,
                    help="Migrations per generation")
    ap.add_argument("--migration-rate", type=float, default=0.1,
                    help="Fraction of population to migrate")
    ap.add_argument("--checkpoint", action="store_true",
                    help="Enable checkpointing")
    ap.add_argument("--checkpoint-interval", type=int, default=10,
                    help="Checkpoint every N generations")
    ap.add_argument("--resume", action="store_true",
                    help="Resume from latest checkpoint")
    ap.add_argument("--early-stop-patience", type=int, default=10,
                    help="Stop if no improvement for N generations")
    ap.add_argument("--early-stop-threshold", type=float, default=1e-6,
                    help="Minimum improvement to reset patience")
    args = ap.parse_args()

    run_ga(args)


if __name__ == "__main__":
    main()
