"""
ga_constraints.py — Constraint system for Genetic Algorithm optimization.

Provides:
  - Budget constraints (maximum deployment cost)
  - Effectiveness constraints (minimum detection/kill probability)
  - Resource constraints (maximum number of units)
  - Zone constraints (units must be placed in specific zones)
  - Composite penalty function for constraint violation
"""

from dataclasses import dataclass, field
from typing import List, Optional, Dict, Any
import numpy as np


@dataclass
class BudgetConstraint:
    """Limit total deployment cost."""
    max_cost: float
    weight: float = 10.0  # Penalty weight per unit cost violation

    def penalty(self, actual_cost: float) -> float:
        if actual_cost <= self.max_cost:
            return 0.0
        return self.weight * (actual_cost - self.max_cost) / self.max_cost


@dataclass
class EffectivenessConstraint:
    """Require minimum effectiveness."""
    min_effectiveness: float
    weight: float = 10.0

    def penalty(self, actual_effectiveness: float) -> float:
        if actual_effectiveness >= self.min_effectiveness:
            return 0.0
        return self.weight * (self.min_effectiveness - actual_effectiveness)


@dataclass
class ResourceConstraint:
    """Limit number of units by type."""
    max_units: Dict[str, int] = field(default_factory=dict)
    weight: float = 5.0

    def penalty(self, unit_counts: Dict[str, int]) -> float:
        total_penalty = 0.0
        for unit_type, max_count in self.max_units.items():
            actual = unit_counts.get(unit_type, 0)
            if max_count > 0 and actual > max_count:
                total_penalty += self.weight * (actual - max_count) / max_count
        return total_penalty


@dataclass
class ZoneConstraint:
    """Require units to be placed in specific zones."""
    required_zones: List[str] = field(default_factory=list)
    weight: float = 8.0

    def penalty(self, units_in_zones: Dict[str, int]) -> float:
        missing = 0
        for zone in self.required_zones:
            if units_in_zones.get(zone, 0) == 0:
                missing += 1
        return self.weight * missing / len(self.required_zones) if self.required_zones else 0.0


class ConstraintSystem:
    """Combines multiple constraints into a single penalty function."""

    def __init__(self):
        self.constraints: List[Any] = []

    def add_budget(self, max_cost: float, weight: float = 10.0) -> None:
        self.constraints.append(BudgetConstraint(max_cost, weight))

    def add_effectiveness(self, min_eff: float, weight: float = 10.0) -> None:
        self.constraints.append(EffectivenessConstraint(min_eff, weight))

    def add_resource(self, max_units: Dict[str, int], weight: float = 5.0) -> None:
        self.constraints.append(ResourceConstraint(max_units, weight))

    def add_zone(self, required_zones: List[str], weight: float = 8.0) -> None:
        self.constraints.append(ZoneConstraint(required_zones, weight))

    def total_penalty(
        self,
        cost: float = 0.0,
        effectiveness: float = 0.0,
        unit_counts: Dict[str, int] = None,
        units_in_zones: Dict[str, int] = None,
    ) -> float:
        """Calculate total constraint violation penalty."""
        unit_counts = unit_counts or {}
        units_in_zones = units_in_zones or {}

        total = 0.0
        for constraint in self.constraints:
            if isinstance(constraint, BudgetConstraint):
                total += constraint.penalty(cost)
            elif isinstance(constraint, EffectivenessConstraint):
                total += constraint.penalty(effectiveness)
            elif isinstance(constraint, ResourceConstraint):
                total += constraint.penalty(unit_counts)
            elif isinstance(constraint, ZoneConstraint):
                total += constraint.penalty(units_in_zones)

        return total

    def is_feasible(
        self,
        cost: float = 0.0,
        effectiveness: float = 0.0,
        unit_counts: Dict[str, int] = None,
    ) -> bool:
        """Check if a solution satisfies all constraints."""
        return self.total_penalty(cost, effectiveness, unit_counts) == 0.0


def apply_constraints_to_fitness(
    base_fitness: float,
    constraint_system: ConstraintSystem,
    **kwargs,
) -> float:
    """
    Adjust fitness by subtracting constraint penalties.

    Args:
        base_fitness: Raw fitness value (higher is better)
        constraint_system: ConstraintSystem instance
        **kwargs: Passed to constraint_system.total_penalty()

    Returns:
        Adjusted fitness (may be negative if heavily constrained)
    """
    penalty = constraint_system.total_penalty(**kwargs)
    return base_fitness - penalty


# Example usage
if __name__ == '__main__':
    cs = ConstraintSystem()
    cs.add_budget(max_cost=500000, weight=10.0)
    cs.add_effectiveness(min_eff=0.8, weight=10.0)
    cs.add_resource(max_units={"detector": 5, "interceptor": 3}, weight=5.0)

    # Example evaluation
    fitness = 0.95
    cost = 750000
    effectiveness = 0.85
    unit_counts = {"detector": 3, "interceptor": 4, "seeker": 10}

    penalty = cs.total_penalty(cost, effectiveness, unit_counts)
    adjusted = apply_constraints_to_fitness(fitness, cs,
                                            cost=cost,
                                            effectiveness=effectiveness,
                                            unit_counts=unit_counts)

    print(f"Base fitness: {fitness:.3f}")
    print(f"Constraint penalty: {penalty:.3f}")
    print(f"Adjusted fitness: {adjusted:.3f}")
    print(f"Feasible: {cs.is_feasible(cost, effectiveness, unit_counts)}")
