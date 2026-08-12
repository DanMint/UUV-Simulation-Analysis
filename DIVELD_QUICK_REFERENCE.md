# Dive-LD Scenario Quick Reference

**Scenario**: `scenarios/diveld_baseline_complete.json`  
**Map**: Pearl Harbour (100×100 shallow-water grid)  
**Type**: Baseline for GA fitness evaluation and loss-exchange-ratio analysis  

---

## Scenario Composition

### Attackers (3× Dive-LD AUVs)

| ID | Position | Direction | Heading | Notes |
|----|----------|-----------|---------|-------|
| 1 | (10, 10) | North | South | Upper-left approach |
| 2 | (65, 45) | South | North | Lower-center approach |
| 3 | (40, 85) | East | West | Right-flank approach |

**Vehicle Specs** (all):
- Type: Dive-LD (Anduril long-endurance AUV)
- Speed: 1–3 knots (stepDelay = 4)
- Detection: Hydrophone-vulnerable (200–400 kHz)
- Cost: $500k–$1M each
- Endurance: 72+ hours
- Shallow-water capable: YES

### Defenders

| Type | Position | Role | Radius | Notes |
|------|----------|------|--------|-------|
| Detector | (30, 45) | North-facing sensor | 4 cells | Detects attackers in range |
| Detector | (40, 40) | West-facing sensor | 4 cells | Covers west approach |
| Interceptor | (35, 50) | Central effector | 3 cells | Kills detected attackers |

**Effectiveness**:
- Kill probability: 90% (close), 60% (mid), 50% (far)
- Engagement cost: $50,000/shot

### Target

| Parameter | Value |
|-----------|-------|
| Position | (35, 45) — Harbor center |
| Type | Shahed (1-way attack drone) |
| Status | Critical asset (`is_critical: true`) |
| Defense | All detectors + interceptor |

---

## Usage Patterns

### Pattern 1: Baseline Single Run (Reproducible)
```powershell
.\uuv_sim.exe --scenario scenarios/diveld_baseline_complete.json `
  --iterations 1 --seed 42 --no-prompt
```

**Use case**: Verify scenario loads, check reproducibility  
**Output**: `runs/run_0.json`, row in `runs/summary.csv`  
**Time**: ~2-5 seconds  

### Pattern 2: GA Parameterization Series
```powershell
.\uuv_sim.exe --scenario scenarios/diveld_baseline_complete.json `
  --iterations 5 --noise-step 0.1 --seed 42 --no-prompt
```

**Use case**: Generate baseline cost-benefit curves across noise levels  
**Output**: `runs/run_0.json` through `runs/run_4.json`, 5 CSV rows  
**Noise range**: 0.0 → 0.4 (5% per iteration)  
**Time**: ~15-30 seconds  

### Pattern 3: Live Visualization
```powershell
.\uuv_sim.exe --scenario scenarios/diveld_baseline_complete.json --visualize
```

**Use case**: Interactive inspection, trail visualization, live debugging  
**Controls**: Space (pause), +/- (speed), L (legend), Escape (exit)  
**Output**: Visual replay, exit on completion  

---

## Output Format

### JSON Result (`runs/run_N.json`)

```json
{
  "scenario_name": "diveld_baseline_complete",
  "map_width": 100,
  "map_height": 100,
  "total_steps": 1847,
  "simulation_time_ms": 3250,
  "seed": 42,
  "noise_level": 0.0,
  
  "seekers": [
    {
      "id": 0,
      "start": [10, 10],
      "end": [35, 45],
      "distance_cells": 50,
      "status": "reached_target",
      "detected": true,
      "killed": false
    }
  ],
  
  "attackers": [
    {
      "id": 0,
      "vehicle_type": "diveld",
      "spawn_row": 10,
      "spawn_col": 10,
      "unit_cost_min": 500000,
      "unit_cost_max": 1000000,
      "mission_success": true
    }
  ],
  
  "targets": [
    {
      "id": 0,
      "row": 35,
      "col": 45,
      "destroyed": true,
      "is_critical": true
    }
  ],
  
  "defenders": [
    {
      "type": "detector",
      "row": 30,
      "col": 45,
      "radius": 4,
      "detections_made": 2
    },
    {
      "type": "interceptor",
      "row": 35,
      "col": 50,
      "radius": 3,
      "engagement_count": 1,
      "engagement_cost": 50000,
      "kills": 0
    }
  ],
  
  "cost_benefit": {
    "blue_cost": 50000,
    "red_cost": 500000,
    "loss_exchange_ratio": 10.0,
    "mission_success_rate": 1.0
  }
}
```

### CSV Row (`runs/summary.csv`)

```csv
run_id,noise_level,blue_cost,red_cost,loss_exchange_ratio,targets_destroyed,total_targets,mission_success_rate,total_steps
0,0.0,50000,500000,10.0,1,1,1.0,1847
1,0.1,50000,500000,10.0,1,1,1.0,1891
2,0.2,100000,500000,5.0,1,1,1.0,1923
3,0.3,150000,500000,3.33,0,1,0.0,2000
4,0.4,200000,500000,2.5,0,1,0.0,2000
```

---

## Analysis Metrics

### Loss-Exchange Ratio (LER)
$$\text{LER} = \frac{\text{Red Cost (deployed attackers)}}{\text{Blue Cost (interceptor engagement)}}$$

- **LER > 1.0**: Defender advantage (blue spends less than red deployed)
- **LER = 1.0**: Breakeven
- **LER < 1.0**: Attacker advantage (blue spends more than red's cost)

### Example Baseline Results

With 3× Dive-LD attackers ($500k each = $1.5M red cost):

| Noise | Blue Cost | LER | Outcome |
|-------|-----------|-----|---------|
| 0.0 | $50k | 30.0 | Strong defense (1 shot kills 1 attacker) |
| 0.1 | $50k | 30.0 | Defense effective |
| 0.2 | $100k | 15.0 | 2 shots needed |
| 0.3 | $150k | 10.0 | Defense degrading |
| 0.4 | $200k | 7.5 | Attacks more likely to succeed |

**Interpretation**: As noise increases, defenders must fire more shots → lower LER → more attacker success.

---

## Customization Guide

### Modify Attacker Positions
Edit `scenarios/diveld_baseline_complete.json`:
```json
"attackers": [
  {
    "spawn_row": 10,    // ← Change this
    "spawn_col": 10,    // ← And this
    "vehicle_type": "diveld"
  }
]
```

### Change Detector/Interceptor Placement
```json
"defenders": [
  {
    "type": "detector",
    "row": 30,  // ← Modify position
    "col": 45,  // ← Modify position
    "radius": 4  // ← Change detection range
  }
]
```

### Adjust Defender Effectiveness
Modify `src/simulation/simulation.cpp`:
```cpp
// Kill probability by range (update constants)
const float KILL_PROB_CLOSE = 0.90f;  // ← Change
const float KILL_PROB_MID   = 0.60f;  // ← Change
const float KILL_PROB_FAR   = 0.50f;  // ← Change
```

### Change Target Position
```json
"targets": [
  {
    "row": 35,  // ← Modify
    "col": 45,  // ← Modify
    "is_critical": true
  }
]
```

---

## Performance Tips

### For Fast Iterations
- Use `--iterations 1 --no-prompt` (skip UI)
- Use fixed seed for reproducibility
- Headless mode (no visualizer)

### For GA Optimization
- Run multiple series with increasing noise
- Store baseline results separately
- Use CSV output for batch analysis

### For Visualization
- Use `--visualize` with small iteration counts
- Pause (Space key) to inspect specific moments
- Toggle legend (L key) for clarity

---

## Integration Points

### For GA Fitness Function
```python
import subprocess
import json

def fitness_diveld_baseline(detector_positions, interceptor_positions):
    """Evaluate fitness of defender placement"""
    # 1. Create modified scenario with new positions
    # 2. Run: uuv_sim.exe --scenario modified.json --iterations 3 --noise-step 0.1
    # 3. Parse runs/summary.csv
    # 4. Return loss_exchange_ratio as fitness (maximize)
    pass
```

### For Comparison Analysis
```python
# Compare multiple scenarios
scenarios = [
    "diveld_baseline_complete.json",
    "diveld_optimized_defenders.json",
    "diveld_swarm_attack.json"
]

for scenario in scenarios:
    results = run_simulation(scenario, iterations=5)
    plot_loss_exchange_ratio(results)
```

---

## Quick Commands

```bash
# Reproducible baseline
uuv_sim --scenario scenarios/diveld_baseline_complete.json --iterations 1 --seed 42 --no-prompt

# GA series
uuv_sim --scenario scenarios/diveld_baseline_complete.json --iterations 10 --noise-step 0.05 --seed 42 --no-prompt

# Visualize
uuv_sim --scenario scenarios/diveld_baseline_complete.json --visualize

# Analyze results
python scripts/visualize.py runs/
python scripts/analyze_costs.py runs/
```

---

## References

- [Full Summary](PHASE_17_DIVELD_SUMMARY.md)
- [Build Guide](PHASE_17_BUILD_DEPLOYMENT_GUIDE.md)
- [Scenario File](scenarios/diveld_baseline_complete.json)
- [Test Source](tests/test_diveld_scenario.cpp)

---
