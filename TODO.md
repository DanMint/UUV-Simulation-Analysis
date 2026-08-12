# Improvements Plan

## Phase 1-10: Complete ✅
All prior phases (critical sim bugs, visualizer, main.cpp, scenario loading, cost-benefit, reproducibility) are complete and verified.

## Phase 11: Critical Asset Flag (Lance's request) ✅
- [x] 11a. `TargetAgent::isCritical` flag (default false)
- [x] 11b. `UnitSpawn::isCritical` + SpawnConfig serialization
- [x] 11c. `Simulation::buildResult()` populates `TargetResult::isCritical`
- [x] 11d. `saveCSV()` uses flagged critical target, not `targetResults[0]`
- [x] 11e. JSON emits `is_critical` per target
- [x] 11f. visualize.py highlights critical target

## Phase 12: GA Framework (Lance's exercise) ✅
- [x] 12a. `scripts/genetic_algorithm.py` — SGA: evaluate → roulette select → crossover → mutate
- [x] 12b. Knapsack objective (max value within weight budget)
- [x] 12c. Reproducible seed, per-generation fitness, convergence plot
- [x] 12d. This is the GA that will later wrap the simulator as fitness function

## Phase 13: Reproducibility Proof ✅
- [x] 13a. Two identical seeded batch runs → byte-identical CSV/JSON
- [x] 13b. Proof documented

## Phase 14: Lance Plain-English Update ✅
- [x] 14a. `DOCS/lance_update.md` with what we built, fixed, and next

## Phase 15: Full-File Review Sweep ✅
- [x] 15a. Read all remaining source files
- [x] 15b. Fix any real bugs found

## Phase 16: GA Real Integration (current sprint) 🔄
- [ ] 16a. `scripts/genetic_algorithm.py` — wrap the UUV simulator as the fitness function
      (call `uuv_sim.exe --repeat N` per chromosome, read `runs/ga_batch.csv`)
- [x] 16b. `main.cpp --repeat N` GA repeat mode writes `runs/ga_batch.csv`
      (P(detected), P(killed), deployment cost, effectiveness per run)
- [ ] 16c. GA defender-optimizer: choose detector/interceptor placements to maximize
      effectiveness subject to a deployment budget
- [ ] 16d. GA attacker-optimizer: choose attacker spawn positions/types to maximise
      target destruction subject to attacker budget
- [ ] 16e. Convergence plots + per-generation best/worst/avg fitness

## Phase 17: Dive-LD Integration & Baseline Scenario (current sprint) 🔄
- [x] 17a. Register Dive-LD specs in `vehicleSpecs.cpp` ✅
- [x] 17b. Create Dive-LD baseline scenario: `scenarios/diveld_baseline_complete.json` ✅
      (3x Dive-LD attackers from north/south/east directions, 1x critical target, 2x detectors + 1x interceptor)
- [x] 17c. Unit test: `tests/test_diveld_scenario.cpp` ✅
- [x] 17d. Update README with Dive-LD documentation ✅
- [ ] 17e. Phase 17 follow-up: Run GA parameterization series for loss-exchange-ratio analysis
- [ ] 17f. GA detector placement optimizer (Phase 16 continuation)
