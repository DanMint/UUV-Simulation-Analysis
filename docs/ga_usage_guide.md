# Genetic Algorithm — Comprehensive Usage Guide

## Overview

The UUV simulation includes a real Genetic Algorithm (GA) that uses the C++ simulator as its fitness function. Each chromosome represents a candidate deployment of defenders or attackers, and its fitness is measured by running actual simulations.

## Architecture

```
Python GA (genetic_algorithm.py)
    │
    ├── Generates chromosomes (placements)
    │
    ├── Writes scenario JSON (chromo_scenario.json)
    │
    ├── Invokes simulator:
    │       uuv_sim.exe --scenario ... --repeat N --seed S --no-prompt
    │
    ├── Reads fitness from:
    │       runs/ga_batch.csv          (batch results)
    │       runs/ga_history.csv        (GA convergence history)
    │       ga_checkpoint.json         (resumable state)
    │
    └── Evolves population via:
            - Tournament selection
            - Single-point crossover
            - Uniform mutation
            - Island model with migration
            - Elitism (best chromosome preserved)
```

## Quick Start

### 1. Build the simulator

```bash
cd windows_build
cmake -B build -DCMAKE_TOOLCHAIN_FILE="C:/vcpkg/scripts/buildsystems/vcpkg.cmake" .
cmake --build build --config Release --target uuv_sim
```

### 2. Prepare a scenario with zones

Zones define where the GA can place units. Use the spawn tool to draw zones:
- Press `Z` to draw attacker zones (red)
- Press `X` to draw defender zones (blue)

Or create zones manually in the scenario JSON:

```json
{
  "defender_zones": [
    {"row_min": 10, "col_min": 10, "row_max": 30, "col_max": 30, "num_detectors": 2, "num_interceptors": 1}
  ],
  "attacker_zones": [
    {"row_min": 60, "col_min": 60, "row_max": 80, "col_max": 80, "num_seekers": 3}
  ]
}
```

### 3. Run the defender optimizer

```bash
python scripts/genetic_algorithm.py \
    --scenario scenarios/diveld_baseline_complete.json \
    --side defender \
    --pop 40 --gens 30 --repeat 5 --seed 1
```

### 4. Run the attacker optimizer

```bash
python scripts/genetic_algorithm.py \
    --scenario scenarios/diveld_baseline_complete.json \
    --side attacker \
    --pop 40 --gens 30 --repeat 5 --seed 1 \
    --n-attackers 3
```

### 5. Analyze results

```bash
python scripts/analyze_ga.py
python scripts/analyze_costs.py runs/
python scripts/visualize.py runs/ scenarios/diveld_baseline_complete.json
```

## Command-Line Options

| Option | Default | Description |
|--------|---------|-------------|
| `--scenario` | (required) | Path to scenario JSON |
| `--side` | `defender` | `defender` or `attacker` |
| `--pop` | `40` | Population size |
| `--gens` | `30` | Number of generations |
| `--repeat` | `5` | Simulator repeats per chromosome (reduces noise) |
| `--seed` | `1` | Random seed for reproducibility |
| `--pm` | `0.15` | Mutation probability |
| `--lambda` | `0.5` | Cost penalty weight in fitness |
| `--budget` | `100.0` | Deployment budget |
| `--n-detectors` | `3` | Number of detectors (defender) |
| `--n-interceptors` | `2` | Number of interceptors (defender) |
| `--n-attackers` | `5` | Number of attackers (attacker) |
| `--sim` | `uuv_sim.exe` | Path to simulator executable |
| `--jobs` | `1` | Parallel evaluation jobs |
| `--islands` | `1` | Number of island sub-populations |
| `--migration-interval` | `5` | Migrate every N generations |
| `--migration-rate` | `0.1` | Fraction of population to migrate |
| `--checkpoint` | `false` | Enable checkpointing |
| `--checkpoint-interval` | `10` | Checkpoint every N generations |
| `--resume` | `false` | Resume from latest checkpoint |
| `--early-stop-patience` | `10` | Stop if no improvement for N generations |
| `--early-stop-threshold` | `1e-6` | Minimum improvement to reset patience |

## Fitness Functions

### Defender Fitness

```
effectiveness = P(detected) * P(killed)
deploy_cost = n_detectors * 1.0 + n_interceptors * 1.0
penalty = lambda * (deploy_cost / budget)
fitness = effectiveness - penalty   (if effectiveness > 0)
```

Where:
- `P(detected)` = fraction of attackers ever detected by any detector
- `P(killed)` = fraction of attackers intercepted by any interceptor
- `deploy_cost` uses the C++ simulator's actual unit costs (1.0 each)

### Attacker Fitness

```
destroy_rate = targets_destroyed / total_targets
red_cost = mean(red_cost from CSV across repeats)
penalty = lambda * (red_cost / budget)
fitness = destroy_rate - penalty   (if destroy_rate > 0)
```

## Chromosome Encoding

### Defender Chromosome

A list of `n_detectors + n_interceptors` integers. Each integer is an index into the flattened list of water cells inside defender zones. The first `n_detectors` values are detector placements; the remaining `n_interceptors` are interceptor placements.

### Attacker Chromosome

A list of `2 * n_attackers` integers. For each attacker `i`:
- `chromo[2*i]` = cell index into attacker zone water cells
- `chromo[2*i + 1]` = vehicle type index into `VEHICLES` list

## Island Model

The GA supports an island model for parallel exploration:

```bash
python scripts/genetic_algorithm.py \
    --scenario scenario.json --side defender \
    --pop 40 --islands 4 --jobs 4 \
    --migration-interval 5 --migration-rate 0.1
```

Each island evolves independently. Every `migration_interval` generations, the top `migration_rate` fraction of each island migrates to the next island. This maintains diversity and prevents premature convergence.

## Checkpointing and Resume

```bash
# Run with checkpointing
python scripts/genetic_algorithm.py \
    --scenario scenario.json --side defender \
    --pop 40 --gens 100 --checkpoint --checkpoint-interval 10

# Resume from checkpoint
python scripts/genetic_algorithm.py \
    --scenario scenario.json --side defender \
    --pop 40 --gens 100 --resume
```

Checkpoints are saved to `ga_checkpoint.json` in the scenario directory.

## Early Stopping

```bash
python scripts/genetic_algorithm.py \
    --scenario scenario.json --side defender \
    --pop 40 --gens 100 \
    --early-stop-patience 15 --early-stop-threshold 1e-5
```

The GA stops if the best fitness hasn't improved by more than `early_stop_threshold` for `early_stop_patience` consecutive generations.

## Parallel Evaluation

```bash
python scripts/genetic_algorithm.py \
    --scenario scenario.json --side defender \
    --pop 40 --gens 30 --jobs 4
```

Uses `concurrent.futures.ProcessPoolExecutor` to evaluate multiple chromosomes concurrently. Each worker spawns its own simulator instance.

**Note:** Parallel mode requires the simulator to be thread-safe and the scenario file to be writable by multiple processes.

## C++ Direct Integration (No Subprocess)

For maximum performance, use `Simulation::runBatch()` directly from C++:

```cpp
#include "simulation.h"
#include "spawnConfig.h"

std::vector<SpawnConfig> configs;
for (int i = 0; i < pop_size; i++) {
    SpawnConfig sc = chromo_to_scenario(base, chromosome[i]);
    configs.push_back(sc);
}

auto results = Simulation::runBatch(configs, 2000, base_seed);

for (const auto& r : results) {
    double fitness = r.probabilityDetected * r.probabilityKilled
                     - lambda * (r.totalDeploymentCost / budget);
}
```

This avoids subprocess overhead and is ~10x faster than the Python subprocess approach.

## Output Files

| File | Description |
|------|-------------|
| `ga_chromo_scenario.json` | Current chromosome's scenario |
| `ga_best_scenario.json` | Best chromosome's scenario |
| `ga_history.csv` | Per-generation fitness statistics |
| `ga_convergence.png` | Fitness convergence plot |
| `ga_diversity.png` | Population diversity plot |
| `ga_checkpoint.json` | Resumable GA state |
| `runs/ga_batch.csv` | Simulator batch output (CSV) |
| `runs/run_N.json` | Simulator batch output (JSON per run) |

## Cost Model

The GA uses the same cost model as the C++ simulator:

| Unit Type | Deployment Cost | Engagement Cost |
|-----------|----------------|-----------------|
| Detector | 1.0 | N/A (sense-only) |
| Interceptor | 1.0 | 250,000 per shot |
| Attacker | Vehicle-specific (from `vehicleSpecs.cpp`) | N/A |

**Important:** The Python GA's cost penalty uses deployment cost only, not engagement cost. The C++ simulator tracks both in `ga_batch.csv`.

## Vehicle Types

| Vehicle | Cost | Type | Speed |
|---------|------|------|-------|
| bluerov2 | $6,000 | UUV | 1-3 kn |
| riptide | $15,000 | UUV | 2-5 kn |
| blueboat | $5,000 | USV | 2-6 kn |
| yuco | $50,000 | UUV | 2-6 kn |
| nemosens | $60,000 | UUV | 2-4 kn |
| hugin | $2,000,000 | UUV | 2-5 kn |
| tb2 | $2,000,000 | UAV | 90-110 kn |
| queenhornet | $1,000 | UAV | 38-43 kn |
| shahed | $20,000 | UAV | 90-100 kn |
| diveld | $500,000 | UUV | 1-3 kn |

## Troubleshooting

| Problem | Solution |
|---------|----------|
| "Error: no defender zones" | Draw zones with `X` in spawn tool, or add `defender_zones` to JSON |
| "Error: no attacker zones" | Draw zones with `Z` in spawn tool, or add `attacker_zones` to JSON |
| Simulator crashes during GA | Check `runs/` for error logs; reduce `--repeat`; try `--jobs 1` |
| GA converges too slowly | Increase `--pop`, increase `--pm`, or use `--islands` |
| GA collapses to single solution | Increase `--pm`, use `--islands 4`, or decrease `--lambda` |
| "No GA batch output" | Ensure `uuv_sim.exe` is built with `--repeat` support |

## Advanced: Custom Fitness Functions

Edit `evaluate_defender()` or `evaluate_attacker()` in `genetic_algorithm.py` to customize fitness. The function receives the CSV rows and returns an `EvalResult`.

Example: Multi-objective fitness weighting detection vs kill probability:

```python
def evaluate_defender_custom(...):
    ...
    p_det = np.mean([r.get("probability_detected", 0.0) for r in rows])
    p_kill = np.mean([r.get("probability_killed", 0.0) for r in rows])
    effectiveness = 0.3 * p_det + 0.7 * p_kill  # weight kills higher
    ...
```

## Advanced: NSGA-II Multi-Objective

The current GA uses single-objective fitness. For multi-objective optimization (e.g., maximize effectiveness AND minimize cost), implement NSGA-II:

1. Use non-dominated sorting instead of fitness ranking
2. Use crowding distance for diversity
3. Return Pareto front instead of single best

This is not yet implemented but the `EvalResult` dataclass already supports multiple objectives (`effectiveness`, `deploy_cost`, `destroy_rate`, `red_cost`).
