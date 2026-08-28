"""
Chromosome = a list of 20 genes representing candidate locations N1..N20.
  - First 10 genes -> hydrophone candidate locations
  - Last 10 genes  -> defender AUV candidate locations

Binary encoding: since each gene's TYPE (hydrophone vs defender AUV) is
already fixed by its position, the gene itself just needs to say whether
that location is occupied.
  0 = nothing placed here
  1 = occupied (hydrophone if index < 10, defender AUV if index >= 10)

Example: [0,0,1,0,1,0,0,0,1,0, 1,1,0,0,1,0,0,0,0,0]
"""

import random

CHROMOSOME_LENGTH = 20
GENE_VALUES = (0, 1)


def random_chromosome():
    """Generate one random chromosome (used to build the initial population)."""
    return [random.choice(GENE_VALUES) for _ in range(CHROMOSOME_LENGTH)]


def random_population(size):
    """Generate `size` random chromosomes."""
    return [random_chromosome() for _ in range(size)]


def crossover(parent_a, parent_b):
    """
    Make one child chromosome by randomly picking each gene from
    parent_a ('mom') or parent_b ('dad'), 50/50 per gene.
    
    Rundown from research of nightmare of reading:
    say parent_a = [1, 0, 1, 1, 0]
        parent_b = [0, 1, 0, 0, 1]
        
    adn the zip fucntion pairs them up in pairs 
    """
    child = []
    for gene_a, gene_b in zip(parent_a, parent_b):
        child.append(gene_a if random.random() < 0.5 else gene_b)
    return child 


def mutate(chromosome):
    """
    Return a mutated COPY of chromosome. Picks one random gene position
    and flips its bit: 0 -> 1 or 1 -> 0.
    """
    mutated = chromosome.copy() #legit .copy() jsut copies a full list/array wild i know 
    idx = random.randrange(CHROMOSOME_LENGTH)
    mutated[idx] = 1 - mutated[idx]  # simple bit flip reason - ensure binary values 
    return mutated