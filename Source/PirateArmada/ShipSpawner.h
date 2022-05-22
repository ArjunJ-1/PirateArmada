// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Boid.h"
#include "GasCloud.h"
#include "Pirate.h"
#include "GameFramework/Actor.h"
#include "ShipSpawner.generated.h"

UCLASS()
class PIRATEARMADA_API AShipSpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AShipSpawner();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, Category = "Entities")
	float MaxShipCount = 300;

	UPROPERTY(EditAnywhere, Category = "Entities")
	float MaxPirateShipCount = 15;
	
	UPROPERTY(EditAnywhere, Category = "Entities")
	TSubclassOf<ABoid> HarvestShip;

	UPROPERTY(EditAnywhere, Category = "Entities")
	TSubclassOf<APirate> PirateShip;

	UPROPERTY(EditAnywhere, Category = "Entities")
	TSubclassOf<AGasCloud> GasCloud;

	int NumofShips = 0;
	int NumOfPredators = 0;
	TArray<AGasCloud*> GasClouds;

	ABoid* HarvestShipSpawn;
	APirate* PredatorShipSpawn;
	void SpawnShip(bool IsHarvester);
	void SetShipVariables(ABoid* Ship);
	void SetPredatorVariables(APirate* Ship);

	//Predator Population
	TArray<DNA> PredatorPopulation;
	TArray<DNA> m_DeadPredatorsDNA;
	int m_NumberOfPredatorGeneration;
	float m_HighestPredatorFitness;
	int NUM_PREDATOR_PARENTS_PAIR = 7;
	float PREDATOR_MUTATION_CHANCE = 0.25f;
	

	// Population Initialization
	TArray<DNA> HarvesterPopulation;
	TArray<DNA> m_DeadDNA;

	int m_NumberOfGeneration;
	float m_HighestFitness;
	int NUM_PARENTS_PAIR = 50;
	float MUTATION_CHANCE = 0.15f;
	void GeneratePopulation(TArray<DNA> newChildren = TArray<DNA>(), TArray<DNA> POPULATION = TArray<DNA>(), bool IsHarvester = false);
	TArray<DNA> ChildGeneration(int PARENT_PAIR, TArray<DNA> POPULATION);
};
