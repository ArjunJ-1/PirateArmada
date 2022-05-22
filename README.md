The Fitness selection works by sorting the best boids, (The ones with the highest fitness score) and producing offspring from then boids. Any boids that are alive have their DNA replaced by the same number of offsprings causing each generation to have new DNA, ready to be calculated for fitness again.

The Fitness strategy used is elitism which is best suitable for larger population with a small cost for the lack of diversity from parent selection offered by strategies like roulette wheel.

The genotype selection in the crossover function works well by randomly picking a midpoint and producing offspring from using the DNA of both parents. This allows for some diversity and randomness in the allocation of DNA for the offsprings.

The mutation chance in the spawner class is set to 0.15 and the mutation chance in the DNA class is set to 0.15, I have decided on these values after much trial and error done in the week 9 lab for evolution of string agents to form a goal string which also uses elitism strategy. A boid that is mutated and achieves a good fitness value will be chosen of breeding and produce offspring with the same mutation, If each generation of this mutated boid achieves a good fitness score then that strength value (DNA value) becomes the norm.

The fitness score for each boid is proportional to the time it has been alive for and more incentive is given to the boids that have collected gold. The incentive for collecting gold should result in the future generation get more gold and resulting in more flocking behaviour. It should be noted that only providing incentive to the boids for being alive and collecting gold would result in non flocking behaviour as the centring value would get closer to 0 by every generation. This is because each boid would evolve to survive longer by not colliding with any other boid and lowering its centring strength. To counter this I have lowered the incentive for being alive and given incentive to boids that have each other in their perception sphere. This should result in the flocking behaviour.

I have also extended the DNA of the harvesters to include RunAwayStrength and a function to increase acceleration in the opposite direction if the harvester encounters any predator in its perception sphere. In addition to the chase function and chase strength implemented for the predators.

Note that both predators and harvesters evolve at the same time, when the population of harvesters is less than 80%. Ensuring the balance of both parties.
