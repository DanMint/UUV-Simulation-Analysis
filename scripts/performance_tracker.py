"""
performance_tracker.py — Performance regression tracking for simulation engine.

Tracks:
  - Steps per second over time
  - Memory usage trends
  - Test execution times
  - Regression detection

Usage:
  python performance_tracker.py --baseline results/baseline.json
  python performance_tracker.py --record  # Record current performance
  python performance_tracker.py --compare results/current.json  # Detect regressions
"""

import json
import time
import os
import sys
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Optional, Tuple
import statistics


class PerformanceTracker:
    """Track and compare simulation performance metrics."""

    def __init__(self, storage_dir: str = "performance"):
        self.storage_dir = Path(storage_dir)
        self.storage_dir.mkdir(exist_ok=True)
        self.metrics_file = self.storage_dir / "metrics.json"
        self.metrics: List[Dict] = []
        self._load()

    def _load(self) -> None:
        """Load historical metrics."""
        if self.metrics_file.exists():
            with open(self.metrics_file, 'r') as f:
                self.metrics = json.load(f)

    def _save(self) -> None:
        """Save metrics to disk."""
        with open(self.metrics_file, 'w') as f:
            json.dump(self.metrics, f, indent=2)

    def record(
        self,
        steps_per_second: float,
        memory_mb: float = 0.0,
        test_name: str = "simulation",
        metadata: Dict = None,
    ) -> Dict:
        """Record a performance measurement."""
        entry = {
            "timestamp": datetime.now().isoformat(),
            "test_name": test_name,
            "steps_per_second": steps_per_second,
            "memory_mb": memory_mb,
            "metadata": metadata or {},
        }
        self.metrics.append(entry)
        self._save()
        return entry

    def get_baseline(self, test_name: str = "simulation", n: int = 5) -> Optional[Dict]:
        """Get baseline performance from last N runs."""
        runs = [m for m in self.metrics if m["test_name"] == test_name]
        if len(runs) < n:
            return None

        recent = runs[-n:]
        sps_values = [r["steps_per_second"] for r in recent]

        return {
            "mean_steps_per_second": statistics.mean(sps_values),
            "median_steps_per_second": statistics.median(sps_values),
            "std_steps_per_second": statistics.stdev(sps_values) if len(sps_values) > 1 else 0,
            "min_steps_per_second": min(sps_values),
            "max_steps_per_second": max(sps_values),
            "n_samples": len(recent),
        }

    def detect_regression(
        self,
        current_sps: float,
        test_name: str = "simulation",
        threshold_pct: float = 10.0,
    ) -> Tuple[bool, str]:
        """
        Detect if current performance is a regression.

        Args:
            current_sps: Current steps per second
            test_name: Test to check against
            threshold_pct: Regression threshold (e.g., 10% = >10% slowdown)

        Returns:
            (is_regression, message)
        """
        baseline = self.get_baseline(test_name)
        if baseline is None:
            return False, "Insufficient baseline data"

        baseline_mean = baseline["mean_steps_per_second"]
        pct_change = ((current_sps - baseline_mean) / baseline_mean) * 100

        if pct_change < -threshold_pct:
            return True, (
                f"REGRESSION: {current_sps:.1f} steps/s is "
                f"{-pct_change:.1f}% slower than baseline "
                f"({baseline_mean:.1f} steps/s)"
            )

        return False, f"OK: {pct_change:+.1f}% vs baseline"

    def compare_files(self, current_file: str, baseline_file: str) -> List[str]:
        """Compare two metric files and report differences."""
        with open(current_file, 'r') as f:
            current = json.load(f)
        with open(baseline_file, 'r') as f:
            baseline = json.load(f)

        messages = []
        for key in ["steps_per_second", "memory_mb"]:
            if key in current and key in baseline:
                change = ((current[key] - baseline[key]) / baseline[key]) * 100
                messages.append(
                    f"{key}: {current[key]:.2f} ({change:+.1f}%)"
                )

        return messages

    def print_summary(self, test_name: str = "simulation") -> None:
        """Print performance summary."""
        runs = [m for m in self.metrics if m["test_name"] == test_name]
        if not runs:
            print("No performance data available")
            return

        sps_values = [r["steps_per_second"] for r in runs]
        print(f"\nPerformance Summary ({test_name}):")
        print(f"  Runs: {len(runs)}")
        print(f"  Mean: {statistics.mean(sps_values):.1f} steps/s")
        print(f"  Median: {statistics.median(sps_values):.1f} steps/s")
        if len(sps_values) > 1:
            print(f"  Std Dev: {statistics.stdev(sps_values):.1f}")
        print(f"  Min: {min(sps_values):.1f} steps/s")
        print(f"  Max: {max(sps_values):.1f} steps/s")

        if len(sps_values) >= 2:
            recent = sps_values[-1]
            old = sps_values[0]
            change = ((recent - old) / old) * 100
            print(f"  Trend: {change:+.1f}% (first to last)")


def main():
    import argparse

    parser = argparse.ArgumentParser(description="Performance regression tracker")
    parser.add_argument("--baseline", help="Baseline metrics file")
    parser.add_argument("--record", action="store_true", help="Record current performance")
    parser.add_argument("--compare", help="Compare against file")
    parser.add_argument("--summary", action="store_true", help="Print performance summary")
    parser.add_argument("--test-name", default="simulation", help="Test name")
    parser.add_argument("--steps-per-second", type=float, help="Steps per second to record")
    args = parser.parse_args()

    tracker = PerformanceTracker()

    if args.record and args.steps_per_second:
        entry = tracker.record(
            steps_per_second=args.steps_per_second,
            test_name=args.test_name,
        )
        print(f"Recorded: {entry['steps_per_second']:.1f} steps/s")

    elif args.baseline and args.steps_per_second:
        is_reg, msg = tracker.detect_regression(
            args.steps_per_second,
            args.test_name,
        )
        print(msg)
        sys.exit(1 if is_reg else 0)

    elif args.compare:
        messages = tracker.compare_files(args.compare, args.baseline or "performance/metrics.json")
        for msg in messages:
            print(msg)

    elif args.summary:
        tracker.print_summary(args.test_name)

    else:
        parser.print_help()


if __name__ == '__main__':
    main()
