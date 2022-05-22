// Fill out your copyright notice in the Description page of Project Settings.


#include "ShipSpawner.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
AShipSpawner::AShipSpawner()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AShipSpawner::BeginPlay()
{
	Super::BeginPlay();
	// Spawn pirates first
	for(int i = 0; i < MaxPirateShipCount; i++)
	{
		SpawnShip(false);
	}
	
	for(int i = 0; i < MaxShipCount; i++)
	{
		SpawnShip(true);
	}

	for(int i = 0; i < 5; i++)
	{
		float xloc = FMath::RandRange(-2500.0f, 2500.0f);
		float yloc = FMath::RandRange(-2500.0f, 2500.0f);
		float zloc = FMath::RandRange(0.0f, 5000.0f);
		FVector loc(xloc, yloc, zloc);
		GasClouds.Add(Cast<AGasCloud>(GetWorld()->SpawnActor(GasCloud, &loc ,&FRotator::ZeroRotator)));
	}
}

// Called every frame
void AShipSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if(NumofShips < MaxShipCount * .2)
	{
		// Stores all the ships in the world
		TArray<AActor*> AliveShips;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABoid::StaticClass(), AliveShips);
		
		// Stores all the Predators(Pirates) in the world
		TArray<AActor*> AlivePredators;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), APirate::StaticClass(), AlivePredators);

		// Generate the next generation
		// we have 2 lists (alive ships with DNA and a list of dead DNA
		// ....
		// Predator Population initialization
		PredatorPopulation.Empty();
		m_HighestPredatorFitness = 0;
		TArray<float> BestPredatorDNA;
		APirate* Predator = nullptr;
		
		// Harvesters population initialization
		HarvesterPopulation.Empty();
		m_HighestFitness = 0;
		TArray<float> BestDNA;
		ABoid* Harvester = nullptr;

		// Calculate fitness of all predators and add them to the predator population
		for (auto Pirate: AlivePredators)
		{
			// get the ship with the highest fitness
			if(Cast<APirate>(Pirate) && Cast<APirate>(Pirate)->shipDNA.m_storedfitness > m_HighestPredatorFitness)
			{
				m_HighestPredatorFitness = Cast<APirate>(Pirate)->shipDNA.m_storedfitness;
				BestPredatorDNA = Cast<APirate>(Pirate)->shipDNA.StrengthValues;
				Pirate = Cast<APirate>(Pirate);
			}

			// Add the harvester ship to the m_population
			PredatorPopulation.Add(Cast<APirate>(Pirate)->shipDNA);
		}

		// Calculate fitness of all Harvesters and add them to the Harvesters population
		for (auto Ship: AliveShips)
		{
			// get the ship with the highest fitness
			if(Cast<ABoid>(Ship)->shipDNA.m_storedfitness > m_HighestFitness)
			{
				m_HighestFitness = Cast<ABoid>(Ship)->shipDNA.m_storedfitness;
				BestDNA = Cast<ABoid>(Ship)->shipDNA.StrengthValues;
				Harvester = Cast<ABoid>(Ship);
			}

			// Add the harvester ship to the m_population
			HarvesterPopulation.Add(Cast<ABoid>(Ship)->shipDNA);
		}
		UE_LOG(LogTemp, Warning, TEXT("Time passes: %f"), Harvester->TimeAlive);

		// Calculate best predator fitness
		UE_LOG(LogTemp, Warning, TEXT("Predator Generation :      %d"), m_NumberOfPredatorGeneration);
		UE_LOG(LogTemp, Warning, TEXT("Predator Highest Fitness : %f"), m_HighestPredatorFitness);
		if (BestPredatorDNA.Num() == 6)
		{
			UE_LOG(LogClass, Log, TEXT("Velocity Strength:   %f"), BestPredatorDNA[0]);
			UE_LOG(LogClass, Log, TEXT("Seperation Strength: %f"), BestPredatorDNA[1]);
			UE_LOG(LogClass, Log, TEXT("Centering Strength:  %f"), BestPredatorDNA[2]);
			UE_LOG(LogClass, Log, TEXT("Avoidance Strength:  %f"), BestPredatorDNA[3]);
			UE_LOG(LogClass, Log, TEXT("GasCloud Strength:   %f"), BestPredatorDNA[4]);
			UE_LOG(LogClass, Log, TEXT("Chase Strength:      %f"), BestPredatorDNA[5]);
		}
		

		PredatorPopulation.Append(m_DeadPredatorsDNA);
		m_DeadPredatorsDNA.Empty();

		// Calculate best harvester fitness
		UE_LOG(LogTemp, Warning, TEXT("Harvester Generation :     %d"), m_NumberOfGeneration);
		UE_LOG(LogTemp, Warning, TEXT("Harvester Highest Fitness: %f"), m_HighestFitness);
		if(BestDNA.Num() == 7)
		{
			UE_LOG(LogClass, Log, TEXT("Velocity Strength:   %f"), BestDNA[0]);
			UE_LOG(LogClass, Log, TEXT("Seperation Strength: %f"), BestDNA[1]);
			UE_LOG(LogClass, Log, TEXT("Centering Strength:  %f"), BestDNA[2]);
			UE_LOG(LogClass, Log, TEXT("Avoidance Strength:  %f"), BestDNA[3]);
			UE_LOG(LogClass, Log, TEXT("GasCloud Strength:   %f"), BestDNA[4]);
			UE_LOG(LogClass, Log, TEXT("Speed Strength:      %f"), BestDNA[5]);
			UE_LOG(LogClass, Log, TEXT("RunAway Strength:    %f"), BestDNA[6]);
		}

		HarvesterPopulation.Append(m_DeadDNA);
		m_DeadDNA.Empty();

		// Generate population of Harvesters and Predators
		GeneratePopulation(ChildGeneration(NUM_PARENTS_PAIR, HarvesterPopulation), HarvesterPopulation, true);
		GeneratePopulation(ChildGeneration(NUM_PREDATOR_PARENTS_PAIR, PredatorPopulation), PredatorPopulation, false);

		// Change the DNA of all Alive ships to the DNA of their Offspring
		for (auto Pirate : AlivePredators)
		{
			Cast<APirate>(Pirate)->shipDNA = PredatorPopulation[0];
			PredatorPopulation.RemoveAt(0);
			Cast<APirate>(Pirate)->SetDNA();
		}
		
		// Change the DNA of all Alive ships to the DNA of their Offspring
		for (auto Ship : AliveShips)
		{
			Cast<ABoid>(Ship)->shipDNA = HarvesterPopulation[0];
			HarvesterPopulation.RemoveAt(0);
			Cast<ABoid>(Ship)->SetDNA();
		}

		// Spawn the rest of the random Boids
		while(NumOfPredators < MaxPirateShipCount)
		{
			SpawnShip(false);
		}
		while(NumofShips < MaxShipCount)
		{
			SpawnShip(true);
		}
	}
}

void AShipSpawner::SpawnShip(bool IsHarvester)
{
	float xloc = FMath::RandRange(-2000.0f, 2000.0f);
	float yloc = FMath::RandRange(-2000.0f, 2000.0f);
	float zloc = FMath::RandRange(500.0f, 4500.0f);
	FVector loc(xloc, yloc, zloc);
	if(IsHarvester)
	{
		ABoid* SpawnedShip = Cast<ABoid>(GetWorld()->SpawnActor(HarvestShip, &loc ,&FRotator::ZeroRotator));
		SpawnedShip->Spawner = this;
		SetShipVariables(SpawnedShip);
		NumofShips++;
	}
	else
	{
		APirate* SpawnedShip = Cast<APirate>(GetWorld()->SpawnActor(PirateShip, &loc ,&FRotator::ZeroRotator));
		SpawnedShip->Spawner = this;
		SetPredatorVariables(SpawnedShip);
		NumOfPredators++;
	}
}


void AShipSpawner::SetShipVariables(ABoid* Ship)
{
	if (HarvesterPopulation.Num() > 0)
	{
		Ship->shipDNA = HarvesterPopulation[0];
		HarvesterPopulation.RemoveAt(0);
		Ship->SetDNA();
	}
	else
	{
		// if not enough DNA in m_population
		Ship->shipDNA = DNA(7);
		Ship->SetDNA();
	}
}

void AShipSpawner::SetPredatorVariables(APirate* Ship)
{
	if (PredatorPopulation.Num() > 0)
	{
		Ship->shipDNA = PredatorPopulation[0];
		PredatorPopulation.RemoveAt(0);
		Ship->SetDNA();
	}
	else
	{
		// if not enough DNA in m_population
		Ship->shipDNA = DNA(6);
		Ship->SetDNA();
	}
}

void AShipSpawner::GeneratePopulation(TArray<DNA> newChildren, TArray<DNA> POPULATION, bool IsHarvester)
{
	if (newChildren.Num() == 0)
	{
		if(IsHarvester)
		{
			for (int i = 0; i < MaxShipCount; i++)
				POPULATION.Add(DNA(7));
		}
		else
		{
			for (int i = 0; i < MaxPirateShipCount; i++)
				POPULATION.Add(DNA(6));
		}
	}
	else
	{
		POPULATION.Empty();
		POPULATION.Append(newChildren);
		if(IsHarvester)
		{
			for (int i = 0; i < MaxShipCount - newChildren.Num(); i++)
				POPULATION.Add(DNA(7));
		}
		else
		{
			for (int i = 0; i < MaxPirateShipCount - newChildren.Num(); i++)
				POPULATION.Add(DNA(6));
		}
	}
	if (IsHarvester)
		m_NumberOfGeneration++;
	else
		m_NumberOfPredatorGeneration++;
}

TArray<DNA> AShipSpawner::ChildGeneration(int PARENT_PAIR, TArray<DNA> POPULATION)
{
	// Sorting the population based on the fitness value (improve this)
	TArray<DNA> parents;
	for (int i = 0; i < PARENT_PAIR*2; i++)
	{
		float highestFitness = 0;
		int dnaIdx = -1;
		for (int j = 0; j < POPULATION.Num(); j++)
		{
			if (POPULATION[j].m_storedfitness > highestFitness)
			{
				highestFitness = POPULATION[j].m_storedfitness;
				dnaIdx = j;
			}
		}
		if(dnaIdx != -1)
		{
			// found our highest fitness dna! store the parent
			parents.Add(POPULATION[dnaIdx]);
			//remove the parent form the population
			POPULATION.RemoveAt(dnaIdx);
		}
	}
	
	// Once sorted pick top 50 and generate offspring
	// create children
	//Set up array of children to be added to new population
	TArray<DNA> newChildren;

	for (int i = 0; i < PARENT_PAIR; i++)
	{
		DNA childOne = parents[i*2 + 0].Crossover(parents[1]);
		DNA childTwo = parents[i*2 + 1].Crossover(parents[0]);
		

		// possible mutate them
		if (FMath::RandRange(0.0f, 1.0f) < MUTATION_CHANCE)
		{
			if (FMath::RandBool())
				childOne.Mutation();
			else
				childTwo.Mutation();
		}
		newChildren.Add(childOne);
		newChildren.Add(childTwo);
	}
	return newChildren;
}
