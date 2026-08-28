"""
Fitness = effectiveness x cost_penalty   (slide 5)

effectiveness = P(detected) x P(killed) x P(survived)     (slide 3)

cost_penalty (slide 4):
  C = total cost of everything placed in the chromosome
  B = budget
  if (B - C) >= 0: penalty f = 1                (no penalty, within budget)
  if (B - C) <  0: penalty f = e^(B - C)         (shrinks the more over budget)
"""

import math

from simulate_real import simulate_chromosome

HYDROPHONE_COST = 1
DEFENDER_AUV_COST = 2
BUDGET = 15  # placeholder - tune later


def compute_cost(chromosome):
    num_hydrophones = chromosome[0:10].count(1) #the .count shows number of occurances of 1 in the first 10 genes, which represent hydrophones so if i have [0,0,1,0,1,0,0,0,1,0] then the count will be 3
    num_defenders = chromosome[10:20].count(1)
    return (num_hydrophones * HYDROPHONE_COST) + (num_defenders * DEFENDER_AUV_COST)


def cost_penalty(cost, budget=BUDGET):
    remaining = budget - cost
    if remaining >= 0:
        return 1.0 #good
    return math.exp(remaining)  # remaining is negative here, so this decays toward 0


def compute_fitness(chromosome, budget=BUDGET):
    """
    Runs the (mock) C++ simulation for this chromosome, then combines
    effectiveness with the cost penalty into one fitness score.
    """
    p_detected, p_killed, p_survived = simulate_chromosome(chromosome)
    effectiveness = p_detected * p_killed * p_survived

    cost = compute_cost(chromosome)
    penalty = cost_penalty(cost, budget)

    fitness = effectiveness * penalty 
    #remove this next print was testing its cool to watch ig  
    #print(f"cost: {cost}, penalty: {penalty}, budget: {budget}, effectiveness: {effectiveness}")
    return fitness