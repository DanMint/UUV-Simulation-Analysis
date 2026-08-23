"""
multi_objective_ga.py — Multi-objective Genetic Algorithm for UUV simulation.

Implements NSGA-II style selection for Pareto-optimal solutions.

Features:
  - Multiple conflicting objectives (e.g., maximize effectiveness, minimize cost)
  - Pareto ranking and crowding distance
  - Elitist archive of non-dominated solutions
  - Binary tournament selection with rank/crowding distance
"""

import copy
import random
import numpy as np
from typing import List, Tuple, Callable, Any


class Individual:
    """Represents a candidate solution in the GA."""

    def __init__(self, chromosome: List[int], objectives: List[float] = None):
        self.chromosome = chromosome
        self.objectives = objectives or []
        self.rank = 0
        self.crowding_distance = 0.0
        self.fitness = 0.0

    def dominates(self, other: 'Individual') -> bool:
        """Check if this individual strictly dominates another (Pareto dominance)."""
        if len(self.objectives) != len(other.objectives):
            raise ValueError("Objective count mismatch")

        at_least_one_better = False
        for s, o in zip(self.objectives, other.objectives):
            if s < o:
                return False
            if s > o:
                at_least_one_better = True
        return at_least_one_better

    def __repr__(self):
        return f"Individual(rank={self.rank}, cd={self.crowding_distance:.3f}, obj={self.objectives})"


class MultiObjectiveGA:
    """NSGA-II style multi-objective genetic algorithm."""

    def __init__(
        self,
        population_size: int = 50,
        n_generations: int = 100,
        crossover_rate: float = 0.8,
        mutation_rate: float = 0.1,
        n_objectives: int = 2,
        seed: int = 42,
    ):
        self.population_size = population_size
        self.n_generations = n_generations
        self.crossover_rate = crossover_rate
        self.mutation_rate = mutation_rate
        self.n_objectives = n_objectives
        self.rng = random.Random(seed)
        np.random.seed(seed)

        self.population: List[Individual] = []
        self.archive: List[Individual] = []  # Elite non-dominated solutions
        self.history: List[dict] = []

    def initialize_population(self, chromo_length: int) -> None:
        """Create random initial population."""
        self.population = [
            Individual([self.rng.randint(0, 100) for _ in range(chromo_length)])
            for _ in range(self.population_size)
        ]

    def evaluate(self, individual: Individual, fitness_fn: Callable) -> None:
        """Evaluate objectives for an individual."""
        individual.objectives = fitness_fn(individual.chromosome)

    def fast_non_dominated_sort(self) -> List[List[Individual]]:
        """NSGA-II fast non-dominated sorting."""
        fronts: List[List[Individual]] = []
        domination_counts = {ind: 0 for ind in self.population}
        dominated_sets = {ind: [] for ind in self.population}

        for i, ind in enumerate(self.population):
            for j, other in enumerate(self.population):
                if i == j:
                    continue
                if ind.dominates(other):
                    dominated_sets[ind].append(other)
                elif other.dominates(ind):
                    domination_counts[ind] += 1

        # First front
        front1 = [ind for ind in self.population if domination_counts[ind] == 0]
        for ind in front1:
            ind.rank = 0
        fronts.append(front1)

        # Subsequent fronts
        front_idx = 0
        while front_idx < len(fronts):
            next_front = []
            for ind in fronts[front_idx]:
                for dominated in dominated_sets[ind]:
                    domination_counts[dominated] -= 1
                    if domination_counts[dominated] == 0:
                        dominated.rank = front_idx + 1
                        next_front.append(dominated)
            if next_front:
                fronts.append(next_front)
            front_idx += 1

        return fronts

    def calculate_crowding_distance(self, front: List[Individual]) -> None:
        """Calculate crowding distance for individuals in a front."""
        if len(front) <= 2:
            for ind in front:
                ind.crowding_distance = float('inf')
            return

        n_obj = len(front[0].objectives)
        for ind in front:
            ind.crowding_distance = 0.0

        for m in range(n_obj):
            front.sort(key=lambda ind: ind.objectives[m])
            min_val = front[0].objectives[m]
            max_val = front[-1].objectives[m]
            range_val = max_val - min_val if max_val > min_val else 1.0

            front[0].crowding_distance = float('inf')
            front[-1].crowding_distance = float('inf')
            for i in range(1, len(front) - 1):
                front[i].crowding_distance += (
                    front[i + 1].objectives[m] - front[i - 1].objectives[m]
                ) / range_val

    def tournament_select(self) -> Individual:
        """Binary tournament selection based on rank and crowding distance."""
        a, b = self.rng.sample(self.population, 2)
        if a.rank != b.rank:
            return a if a.rank < b.rank else b
        return a if a.crowding_distance > b.crowding_distance else b

    def crossover(self, parent1: Individual, parent2: Individual) -> Tuple[Individual, Individual]:
        """Single-point crossover."""
        if self.rng.random() > self.crossover_rate:
            return copy.deepcopy(parent1), copy.deepcopy(parent2)

        point = self.rng.randint(1, len(parent1.chromosome) - 1)
        child1_chromo = parent1.chromosome[:point] + parent2.chromosome[point:]
        child2_chromo = parent2.chromosome[:point] + parent1.chromosome[point:]
        return Individual(child1_chromo), Individual(child2_chromo)

    def mutate(self, individual: Individual, mutation_scale: float = 0.1) -> None:
        """Gaussian mutation."""
        for i in range(len(individual.chromosome)):
            if self.rng.random() < self.mutation_rate:
                noise = int(self.rng.gauss(0, 100 * mutation_scale))
                individual.chromosome[i] = max(0, min(100, individual.chromosome[i] + noise))

    def evolve(self, fitness_fn: Callable, chromo_length: int) -> List[Individual]:
        """Run the multi-objective GA."""
        self.initialize_population(chromo_length)

        # Evaluate initial population
        for ind in self.population:
            self.evaluate(ind, fitness_fn)

        for gen in range(self.n_generations):
            # Non-dominated sorting
            fronts = self.fast_non_dominated_sort()

            # Calculate crowding distances
            for front in fronts:
                self.calculate_crowding_distance(front)

            # Update archive with best non-dominated solutions
            current_front = fronts[0]
            for ind in current_front:
                if not any(e.dominates(ind) for e in self.archive):
                    self.archive = [e for e in self.archive if not ind.dominates(e)]
                    self.archive.append(copy.deepcopy(ind))

            # Create offspring
            offspring = []
            while len(offspring) < self.population_size:
                parent1 = self.tournament_select()
                parent2 = self.tournament_select()
                child1, child2 = self.crossover(parent1, parent2)
                self.mutate(child1)
                self.mutate(child2)
                offspring.extend([child1, child2])

            # Evaluate offspring
            for ind in offspring[:self.population_size]:
                self.evaluate(ind, fitness_fn)

            # Combine and select
            combined = self.population + offspring[:self.population_size]
            fronts = self.fast_non_dominated_sort()
            for front in fronts:
                self.calculate_crowding_distance(front)

            new_population = []
            for front in fronts:
                if len(new_population) + len(front) <= self.population_size:
                    new_population.extend(front)
                else:
                    front.sort(key=lambda ind: ind.crowding_distance, reverse=True)
                    new_population.extend(front[:self.population_size - len(new_population)])
                    break

            self.population = new_population

            # Record history
            self.history.append({
                'generation': gen,
                'archive_size': len(self.archive),
                'front_sizes': [len(f) for f in fronts[:3]],
            })

        return self.archive

    def get_pareto_front(self) -> List[Individual]:
        """Get the current Pareto front (non-dominated solutions)."""
        fronts = self.fast_non_dominated_sort()
        return fronts[0] if fronts else []

    def plot_convergence(self, output_path: str = None) -> None:
        """Plot Pareto front evolution (requires matplotlib)."""
        try:
            import matplotlib.pyplot as plt
        except ImportError:
            print("matplotlib required for plotting")
            return

        fig, axes = plt.subplots(1, 2, figsize=(12, 5))

        # Archive size over time
        gens = [h['generation'] for h in self.history]
        archive_sizes = [h['archive_size'] for h in self.history]
        axes[0].plot(gens, archive_sizes)
        axes[0].set_xlabel('Generation')
        axes[0].set_ylabel('Pareto Archive Size')
        axes[0].set_title('Archive Convergence')
        axes[0].grid(True)

        # Final Pareto front (2D only)
        if self.n_objectives == 2 and self.archive:
            obj1 = [ind.objectives[0] for ind in self.archive]
            obj2 = [ind.objectives[1] for ind in self.archive]
            axes[1].scatter(obj1, obj2, c='blue', alpha=0.7)
            axes[1].set_xlabel('Objective 1')
            axes[1].set_ylabel('Objective 2')
            axes[1].set_title('Pareto Front')
            axes[1].grid(True)

        plt.tight_layout()
        if output_path:
            plt.savefig(output_path, dpi=150)
        plt.show()


def demo_fitness(chromosome: List[int]) -> List[float]:
    """Demo fitness function with two conflicting objectives."""
    # Objective 1: Maximize sum (performance)
    obj1 = sum(chromosome) / 1000.0

    # Objective 2: Minimize variance (stability) - negate for maximization
    obj2 = -np.var(chromosome) / 10000.0

    return [obj1, obj2]


if __name__ == '__main__':
    ga = MultiObjectiveGA(
        population_size=50,
        n_generations=50,
        n_objectives=2,
        seed=42,
    )

    archive = ga.evolve(demo_fitness, chromo_length=20)

    print(f"\nPareto Archive: {len(archive)} solutions")
    for i, ind in enumerate(archive[:5]):
        print(f"  [{i}] obj={ind.objectives}, chromo={ind.chromosome[:5]}...")

    ga.plot_convergence()
