

"""
Real version of simulate_chromosome(), wired to the actual C++ simulator.
Same signature as the mock: takes a chromosome, returns
(p_detected, p_killed, p_survived) as floats. fitness.py and ga.py need
ZERO changes - only this file's guts differ from the mock.

To use after using the "GA mode (press Q) and after getting the "Results saved to runs...." 
use: python3 GA/ga.py 
This gives us the same Gen x, best fitness score (Note its the same looking for 1 value and logic we see in the ppt), and chromosome 
Remember the chromosome is first 10 is defender (Hydrophones) then remaing 10 ie 11-20 is interceptor (Defendt UUV)

"""

import json  #reads the json files its this py talking to json files - buuilds 
import subprocess #Python runs a seperate program 
import glob #finds files matching name 
import os #use only newest files

SIM_EXECUTABLE = "./build/uuv_sim"
SCENARIO_PATH = "scenario.json"
CANDIDATES_PER_TYPE = 10 #this is the hard limit of I have 10 defenders(hydrophones)


def _generate_candidate_positions(zone, count):
    """
    Given one defender zone dict (row_min/col_min/row_max/col_max, as saved
    by SpawnConfig::saveJSON), generate "count" candidate (row, col)
    positions spread evenly across the zone's rectangle.

    """
    row_min, col_min = zone["row_min"], zone["col_min"]
    row_max, col_max = zone["row_max"], zone["col_max"]

    width = col_max - col_min + 1
    height = row_max - row_min + 1
    total_cells = width * height

    positions = []
    for i in range(count):
        # spread the `count` requested positions evenly across all cells
        # in the zone, in row-major (left-to-right, top-to-bottom) order
        cell_index = round(i * total_cells / count) if count > 0 else 0
        cell_index = min(cell_index, total_cells - 1)
        row = row_min + cell_index // width #need // its floor division ie whole number 7/3=2
        col = col_min + cell_index % width
        positions.append((row, col))

    return positions


def _chromosome_to_scenario(chromosome, path=SCENARIO_PATH):
    """
    Loads a base scenario (map, targets, zones) and adds detector/interceptor
    units based on the chromosome's genes, using real defender zone bounds
    from the template instead of placeholder coordinates.
    """
    with open(path) as f:
        scenario = json.load(f)

    defender_zones = scenario.get("defender_zones", [])
    if not defender_zones:
        raise ValueError(
            "scenario.json has no defender_zones - draw one with "
            "the X key in GA prep mode before using this."
        )

    zone = defender_zones[0]

    hydrophone_positions = _generate_candidate_positions(zone, CANDIDATES_PER_TYPE)
    defender_positions = _generate_candidate_positions(zone, CANDIDATES_PER_TYPE)

    units = scenario.setdefault("units", [])

    for i, gene in enumerate(chromosome[0:10]):
        if gene == 1:
            row, col = hydrophone_positions[i]
            units.append({"category": "detector", "type": "basic", "row": row, "col": col})

    for i, gene in enumerate(chromosome[10:20]):
        if gene == 1:
            row, col = defender_positions[i]
            units.append({"category": "interceptor", "type": "basic", "row": row, "col": col})

    return scenario


def simulate_chromosome(chromosome, num_runs=1):

    scenario = _chromosome_to_scenario(chromosome)

    tmp_path = "tmp_chromosome_scenario.json"
    with open(tmp_path, "w") as f:
        json.dump(scenario, f)

    """
    Subprocess is essnlly re-runnign that sim in the form of [program, flag, filename] where this time we need to have the GA code re run the code and its changing and giving us the best possible chromosmes think "if i kept the defender at these coords and never changed it there prob isnt a difference BUT if we re run and change those each time then results could be better"
    
    
    Also since we have the whole "Press Q for GA mode" thats what this is doing in said region give new values 
    
    Future Add on is to ask where in teh best chromosome are you actully placing those agents and what those look like exaclty 
    """
    
    subprocess.run(
        [SIM_EXECUTABLE, "--scenario", tmp_path],
        input="1\n",
        text=True,
        capture_output=True,
        check=True,
    )

    """
    Find the most recently written result in runs/ --- this is wher glob and os are key 
    as glob grabs EVERY file in folder then we need to specify which is newest whcih is where we need to use os whcih says get ONLY LOOK AT NEWEST IE LAST MODIFIDED in the form of getmtime
    """
    result_files = sorted(glob.glob("runs/*.json"), key=os.path.getmtime)
    if not result_files:
        raise RuntimeError("No result file found in runs/ after simulation run")

    with open(result_files[-1]) as f:
        result = json.load(f)

    summary = result["summary"]

    total_seekers = summary["total_seekers"]
    total_targets = summary["total_targets"]

    p_detected = summary["seekers_detected"] / total_seekers if total_seekers else 0.0
    p_killed = summary["seekers_intercepted"] / total_seekers if total_seekers else 0.0
    p_survived = 1.0 - (summary["targets_destroyed"] / total_targets) if total_targets else 0.0

    return p_detected, p_killed, p_survived