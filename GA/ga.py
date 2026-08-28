"""
The GA loop (slides 7-12):

  Gen 0: 20 random chromosomes
  Every generation:
    - score every chromosome with compute_fitness()
    - keep top 2, kill the rest
    - crossover top 2 -> 8 children
    - mutate 2 of those 8 children
    - new population = 2 parents + 8 children = 10 chromosomes
  Repeat for 10 generations
"""

import random

from chromosome import random_population, crossover, mutate
from fitness import compute_fitness

INITIAL_POP_SIZE = 20
NEXT_GEN_POP_SIZE = 10
NUM_CHILDREN = 8
NUM_MUTANTS = 2
NUM_GENERATIONS = 10

print("TEST - script is running")

def score_population(population): #need to find a way to evaluat whats the best ie whats mutating that chromosome and its fitness score yea ngl no idea i had trouible here  
    """Returns a list of (chromosome, fitness) sorted best-first."""
    scored = [(chrom, compute_fitness(chrom)) for chrom in population]
    scored.sort(key=lambda pair: pair[1], reverse=True)
    return scored


def make_next_generation(top_two):
    """
    top_two: [chromosome, chromosome] - the 2 fittest parents.
    Returns the new population of 10: the 2 parents + 8 children
    (2 of which are mutated).
    """
    parent_a, parent_b = top_two

    children = [crossover(parent_a, parent_b) for _ in range(NUM_CHILDREN)]

    # mutate 2 of the 8 children, picked at random
    mutant_indices = random.sample(range(NUM_CHILDREN), NUM_MUTANTS)
    for idx in mutant_indices:
        children[idx] = mutate(children[idx])

    return [parent_a, parent_b] + children

#ight all the funciton that use these essenlly cosntatn varibales im essenlly "RENAMING" - START of ga - Functon reasons we go for rand pop to get the ransom chromosoem 20(chromosomes)x20(genes)=400 maths dones??? idk what to name it --- SCORE used to evaulte best chromosmes then COMPUTE to get fitness within this we simulate chromomse whilst it would be used to strip the JSON file for info rn we use the mock code for cpp in py (alot of functions get called here func1>func2>funcX its insane) --- MAKE NEST GEN with we need to do teh CROSSOVER to make child > MUTATE to take 2 children and mutate them 
def run_ga(num_generations=NUM_GENERATIONS, verbose=True):
    population = random_population(INITIAL_POP_SIZE)
    best_per_generation = []

    for gen in range(num_generations):
        scored = score_population(population)
        best_chrom, best_fit = scored[0]
        best_per_generation.append((best_chrom, best_fit))

        if verbose:
            print(f"Gen {gen}: best fitness = {best_fit:.4f}  chromosome = {best_chrom}")

        top_two = [scored[0][0], scored[1][0]]
        population = make_next_generation(top_two)

    return best_per_generation #just put value into list/array thats it yea need to make dumb comments 


if __name__ == "__main__":
    run_ga()