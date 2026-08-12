#!/usr/bin/env python3
"""Apply approved Phase 16/17 enhancements to the UUV simulation.

1. main.cpp: expand ga_batch.csv columns with target-destruction + cost data.
2. genetic_algorithm.py: fix attacker fitness, strip seekers, add diveld,
   guard empty pool.
3. simResult.cpp: fix mis-indented redCost line (cosmetic).
"""
import io
import re

def read(p):
    with io.open(p, encoding='utf-8') as f:
        return f.read()

def write(p, s):
    with io.open(p, 'w', encoding='utf-8', newline='\n') as f:
        f.write(s)

# ─────────────────────────────────────────────────────────────────────
# 1. main.cpp — expand ga_batch.csv header
# ─────────────────────────────────────────────────────────────────────
p = 'src/main.cpp'
s = read(p)
old_hdr = ('hdr << "run_id,probability_detected,probability_killed,"\n'
           '                           "total_deployment_cost,effectiveness\\n";')
new_hdr = ('hdr << "run_id,probability_detected,probability_killed,"\n'
           '                           "total_deployment_cost,effectiveness,"\n'
           '                           "targets_destroyed,total_targets,"\n'
           '                           "blue_cost,red_cost,loss_exchange_ratio\\n";')
if old_hdr in s:
    s = s.replace(old_hdr, new_hdr)
    print('main.cpp: header expanded')
else:
    print('main.cpp: header pattern NOT found')

# Expand the per-run row written to ga_batch.csv
old_row = ('ga << r << ","\n'
           '                       << result.probabilityDetected << ","\n'
           '                       << result.probabilityKilled << ","\n'
           '                       << result.totalDeploymentCost << ","\n'
           '                       << effectiveness << "\\n";')
new_row = ('ga << r << ","\n'
           '                       << result.probabilityDetected << ","\n'
           '                       << result.probabilityKilled << ","\n'
           '                       << result.totalDeploymentCost << ","\n'
           '                       << effectiveness << ","\n'
           '                       << result.targetsDestroyed << ","\n'
           '                       << static_cast<int>(result.targetResults.size()) << ","\n'
           '                       << result.blueCost << ","\n'
           '                       << result.redCost << ","\n'
           '                       << result.lossExchangeRatio << "\\n";')
if old_row in s:
    s = s.replace(old_row, new_row)
    print('main.cpp: row expanded')
else:
    print('main.cpp: row pattern NOT found')

write(p, s)

# ─────────────────────────────────────────────────────────────────────
# 2. genetic_algorithm.py
# ─────────────────────────────────────────────────────────────────────
p = 'scripts/genetic_algorithm.py'
s = read(p)

# 2a. Add diveld to VEHICLES registry
old_reg = '"shahed":      {"cost_min": 20000,   "aerial": True,  "surface": False},\n}'
new_reg = ('"shahed":      {"cost_min": 20000,   "aerial": True,  "surface": False},\n'
           '    "diveld":      {"cost_min": 500000, "aerial": False, "surface": False},\n'
           '}')
if old_reg in s:
    s = s.replace(old_reg, new_reg)
    print('genetic_algorithm.py: registered diveld')
else:
    print('genetic_algorithm.py: registry pattern NOT found')

# 2b. Strip seekers too in attacker scenario
old_strip = '    sc["units"] = [u for u in sc.get("units", [])\n                   if u.get("type") not in ("attacker",)]'
new_strip = '    sc["units"] = [u for u in sc.get("units", [])\n                   if u.get("type") not in ("attacker", "seeker")]'
if old_strip in s:
    s = s.replace(old_strip, new_strip)
    print('genetic_algorithm.py: attacker strips seekers too')
else:
    print('genetic_algorithm.py: strip pattern NOT found')

# 2c. Guard empty pool in both chromo_to_scenario functions
old_def = '    for i in range(n_det):\n        idx = chromo[i] % len(pool)'
new_def = '    if not pool:\n        raise ValueError("Defender zone has no water cells to place detectors")\n\n    for i in range(n_det):\n        idx = chromo[i] % len(pool)'
if old_def in s:
    s = s.replace(old_def, new_def)
    print('genetic_algorithm.py: defender pool guard added')
else:
    print('genetic_algorithm.py: defender pool guard pattern NOT found')

old_atk = '    for i in range(n_atk):\n        cell_idx = chromo[2 * i] % len(pool)'
new_atk = '    if not pool:\n        raise ValueError("Attacker zone has no water cells to place attackers")\n\n    for i in range(n_atk):\n        cell_idx = chromo[2 * i] % len(pool)'
if old_atk in s:
    s = s.replace(old_atk, new_atk)
    print('genetic_algorithm.py: attacker pool guard added')
else:
    print('genetic_algorithm.py: attacker pool guard pattern NOT found')

# 2d. Fix attacker fitness to use real target destruction
old_fit = '''    # Attacker fitness: approximate from ga_batch (which stores P(detected)
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
    return fitness, cost'''
new_fit = '''    # Attacker fitness: measured from real simulated outcomes.
    # ga_batch.csv (Phase 16) now carries per-run target destruction so the
    # attacker optimiser can reward actually destroying the harbour asset(s),
    # not a guessed survival proxy. We also account for attacker unit cost
    # (red_cost from the CSV) so cheap swarm attackers score better.
    t_destroyed = np.sum([r.get("targets_destroyed", 0) for r in rows])
    t_total = np.sum([r.get("total_targets", 0) for r in rows])
    destroy_rate = t_destroyed / max(t_total, 1e-9)

    cost = float(np.mean([r.get("red_cost", 0.0) for r in rows]))

    penalty = lam * (cost / max(budget, 1e-9))
    fitness = destroy_rate - penalty if destroy_rate > 0 else 0.0
    return fitness, cost'''
if old_fit in s:
    s = s.replace(old_fit, new_fit)
    print('genetic_algorithm.py: attacker fitness fixed')
else:
    print('genetic_algorithm.py: attacker fitness pattern NOT found')

# 2e. Update the report print for attacker to reflect destroy rate
old_print = '''    else:
        print(f"  Attacker survival proxy: {details[best_idx][1]:.4f}")
        print(f"  Attacker cost: {details[best_idx][2]:.2f}")'''
new_print = '''    else:
        print(f"  Target destruction rate: {details[best_idx][1]:.4f}")
        print(f"  Attacker cost (red): {details[best_idx][2]:.2f}")'''
if old_print in s:
    s = s.replace(old_print, new_print)
    print('genetic_algorithm.py: attacker report print updated')
else:
    print('genetic_algorithm.py: attacker report print pattern NOT found')

write(p, s)

# ─────────────────────────────────────────────────────────────────────
# 3. simResult.cpp — fix mis-indented redCost line (cosmetic)
# ─────────────────────────────────────────────────────────────────────
p = 'src/simulation/simResult.cpp'
s = read(p)
old_ind = 'redCost += a.unitCostMin;'
new_ind = '        redCost += a.unitCostMin;'
if old_ind in s:
    s = s.replace(old_ind, new_ind)
    print('simResult.cpp: redCost indentation fixed')
else:
    print('simResult.cpp: redCost indentation pattern NOT found')
write(p, s)

print('\nAll edits applied.')
