#!/usr/bin/env python3
"""
uuv_sim_cli.py — Command-line interface for UUV Simulation Analysis.

Subcommands:
    run       Run a simulation scenario
    batch     Run batch simulations
    analyze   Analyze simulation results
    visualize Launch visualization
    ga        Run genetic algorithm optimization
    benchmark Run performance benchmarks
    validate  Validate scenario files
    convert   Convert between scenario formats
"""

import argparse
import sys
import os
import json
import subprocess
import time
from pathlib import Path


def get_sim_exe() -> str:
    """Find the uuv_sim executable."""
    script_dir = Path(__file__).parent
    candidates = [
        script_dir.parent / "windows_build" / "build" / "Release" / "uuv_sim.exe",
        script_dir.parent / "build" / "uuv_sim",
        script_dir.parent / "build" / "Release" / "uuv_sim",
        "uuv_sim",
    ]
    for c in candidates:
        if c.exists() or (c == Path("uuv_sim")):
            return str(c)
    # Try PATH
    for path in os.environ.get("PATH", "").split(os.pathsep):
        candidate = Path(path) / "uuv_sim.exe"
        if candidate.exists():
            return str(candidate)
    return "uuv_sim"


def cmd_run(args: argparse.Namespace) -> int:
    """Run a single simulation scenario."""
    exe = get_sim_exe()
    cmd = [exe, "--scenario", args.scenario]
    if args.repeat > 1:
        cmd.extend(["--repeat", str(args.repeat)])
    if args.seed:
        cmd.extend(["--seed", str(args.seed)])
    if args.no_prompt:
        cmd.append("--no-prompt")
    if args.visualize:
        cmd.append("--visualize")
    if args.max_steps:
        cmd.extend(["--max-steps", str(args.max_steps)])

    print(f"Running: {' '.join(cmd)}")
    start = time.time()
    result = subprocess.run(cmd, capture_output=not args.verbose, text=True)
    elapsed = time.time() - start

    if result.returncode != 0:
        print(f"ERROR: Simulation failed with code {result.returncode}", file=sys.stderr)
        if result.stderr:
            print(result.stderr, file=sys.stderr)
        return 1

    print(f"Completed in {elapsed:.2f}s")
    if args.verbose and result.stdout:
        print(result.stdout)
    return 0


def cmd_batch(args: argparse.Namespace) -> int:
    """Run batch simulations."""
    from scripts.ga_batch_cpp import batch_simulate, write_csv

    print(f"Running batch of {args.count} simulations...")
    start = time.time()

    try:
        results = batch_simulate(
            sim_exe=get_sim_exe(),
            scenario_path=args.scenario,
            count=args.count,
            repeat=args.repeat,
            seed=args.seed,
            output_csv=args.output,
        )
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1

    elapsed = time.time() - start
    print(f"Batch complete: {len(results)} results in {elapsed:.2f}s")
    if args.output:
        print(f"Results written to {args.output}")
    return 0


def cmd_analyze(args: argparse.Namespace) -> int:
    """Analyze simulation results."""
    if not args.csv:
        print("ERROR: --csv required for analyze", file=sys.stderr)
        return 1

    from scripts.analyze_costs import analyze_csv
    try:
        analyze_csv(args.csv, args.output)
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1
    return 0


def cmd_visualize(args: argparse.Namespace) -> int:
    """Launch visualization."""
    if not args.scenario:
        print("ERROR: --scenario required for visualize", file=sys.stderr)
        return 1

    exe = get_sim_exe()
    cmd = [exe, "--scenario", args.scenario, "--visualize"]
    if args.max_steps:
        cmd.extend(["--max-steps", str(args.max_steps)])

    print(f"Launching visualization: {' '.join(cmd)}")
    result = subprocess.run(cmd)
    return result.returncode


def cmd_ga(args: argparse.Namespace) -> int:
    """Run genetic algorithm optimization."""
    from scripts.genetic_algorithm import run_ga

    side = args.side
    scenario = args.scenario
    generations = args.generations
    pop_size = args.population
    seed = args.seed

    print(f"Running {side} GA: {generations} generations, pop={pop_size}")
    start = time.time()

    try:
        run_ga(
            side=side,
            scenario_path=scenario,
            generations=generations,
            population_size=pop_size,
            seed=seed,
            output_dir=args.output_dir,
        )
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1

    elapsed = time.time() - start
    print(f"GA complete in {elapsed:.1f}s")
    return 0


def cmd_benchmark(args: argparse.Namespace) -> int:
    """Run performance benchmarks."""
    from scripts.benchmark_ga import main as benchmark_main
    try:
        return benchmark_main(["--quick" if args.quick else "--full"])
    except SystemExit as e:
        return e.code if isinstance(e.code, int) else (0 if e.code is None else 1)
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1


def cmd_validate(args: argparse.Namespace) -> int:
    """Validate scenario files."""
    if not args.scenario:
        print("ERROR: --scenario required for validate", file=sys.stderr)
        return 1

    try:
        with open(args.scenario, 'r') as f:
            data = json.load(f)
    except json.JSONDecodeError as e:
        print(f"ERROR: Invalid JSON: {e}", file=sys.stderr)
        return 1
    except FileNotFoundError:
        print(f"ERROR: File not found: {args.scenario}", file=sys.stderr)
        return 1

    errors = []
    if 'map' not in data:
        errors.append("Missing 'map' key")
    if 'units' not in data:
        errors.append("Missing 'units' key")

    for i, unit in enumerate(data.get('units', [])):
        if 'type' not in unit:
            errors.append(f"Unit {i}: missing 'type'")
        if 'row' not in unit or 'col' not in unit:
            errors.append(f"Unit {i}: missing 'row' or 'col'")
        valid_types = {'seeker', 'target', 'detector', 'interceptor', 'attacker'}
        if unit.get('type') not in valid_types:
            errors.append(f"Unit {i}: invalid type '{unit.get('type')}'")

    if errors:
        print(f"Validation FAILED ({len(errors)} errors):")
        for e in errors:
            print(f"  - {e}")
        return 1

    print(f"Validation PASSED: {args.scenario}")
    print(f"  Units: {len(data.get('units', []))}")
    return 0


def cmd_convert(args: argparse.Namespace) -> int:
    """Convert between scenario formats."""
    if not args.input or not args.output:
        print("ERROR: --input and --output required for convert", file=sys.stderr)
        return 1

    try:
        with open(args.input, 'r') as f:
            data = json.load(f)
        with open(args.output, 'w') as f:
            json.dump(data, f, indent=2)
        print(f"Converted {args.input} -> {args.output}")
        return 0
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1


def main():
    parser = argparse.ArgumentParser(
        prog='uuv_sim_cli',
        description='UUV Simulation Analysis CLI',
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    subparsers = parser.add_subparsers(dest='command', help='Available commands')

    # run
    p_run = subparsers.add_parser('run', help='Run a simulation scenario')
    p_run.add_argument('scenario', help='Path to scenario JSON file')
    p_run.add_argument('--repeat', type=int, default=1, help='Number of repeats')
    p_run.add_argument('--seed', type=int, default=None, help='RNG seed')
    p_run.add_argument('--no-prompt', action='store_true', help='Skip prompt')
    p_run.add_argument('--visualize', action='store_true', help='Show visualization')
    p_run.add_argument('--max-steps', type=int, default=2000, help='Max simulation steps')
    p_run.add_argument('-v', '--verbose', action='store_true', help='Verbose output')

    # batch
    p_batch = subparsers.add_parser('batch', help='Run batch simulations')
    p_batch.add_argument('scenario', help='Path to scenario JSON file')
    p_batch.add_argument('--count', type=int, default=10, help='Number of simulations')
    p_batch.add_argument('--repeat', type=int, default=1, help='Repeats per simulation')
    p_batch.add_argument('--seed', type=int, default=0, help='Base RNG seed')
    p_batch.add_argument('--output', default='results.csv', help='Output CSV path')

    # analyze
    p_analyze = subparsers.add_parser('analyze', help='Analyze simulation results')
    p_analyze.add_argument('csv', help='Path to results CSV')
    p_analyze.add_argument('--output', '-o', help='Output plot path')

    # visualize
    p_vis = subparsers.add_parser('visualize', help='Launch visualization')
    p_vis.add_argument('scenario', help='Path to scenario JSON file')
    p_vis.add_argument('--max-steps', type=int, default=2000, help='Max simulation steps')

    # ga
    p_ga = subparsers.add_parser('ga', help='Run genetic algorithm')
    p_ga.add_argument('side', choices=['attacker', 'defender'], help='GA side')
    p_ga.add_argument('scenario', help='Path to scenario JSON file')
    p_ga.add_argument('--generations', type=int, default=50, help='Number of generations')
    p_ga.add_argument('--population', type=int, default=30, help='Population size')
    p_ga.add_argument('--seed', type=int, default=42, help='RNG seed')
    p_ga.add_argument('--output-dir', default='.', help='Output directory')

    # benchmark
    p_bench = subparsers.add_parser('benchmark', help='Run benchmarks')
    p_bench.add_argument('--quick', action='store_true', help='Quick benchmark')
    p_bench.add_argument('--full', action='store_true', help='Full benchmark')

    # validate
    p_val = subparsers.add_parser('validate', help='Validate scenario file')
    p_val.add_argument('scenario', help='Path to scenario JSON file')

    # convert
    p_conv = subparsers.add_parser('convert', help='Convert scenario format')
    p_conv.add_argument('input', help='Input file')
    p_conv.add_argument('output', help='Output file')

    args = parser.parse_args()

    if args.command is None:
        parser.print_help()
        return 0

    commands = {
        'run': cmd_run,
        'batch': cmd_batch,
        'analyze': cmd_analyze,
        'visualize': cmd_visualize,
        'ga': cmd_ga,
        'benchmark': cmd_benchmark,
        'validate': cmd_validate,
        'convert': cmd_convert,
    }

    return commands[args.command](args)


if __name__ == '__main__':
    sys.exit(main())
