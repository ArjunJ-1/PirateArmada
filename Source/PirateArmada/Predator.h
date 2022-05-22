// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Boid.h"
#include "Predator.generated.h"

/**
 * 
 */
UCLASS()
class PIRATEARMADA_API APredator : public ABoid
{
	GENERATED_BODY()
public:
	APredator();
	float ChaseStrength = 1000;
	float PredatorTimeAlive;
protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaSeconds) override;

	//apply to the ship and update movement
	virtual void FlightPath(float DeltaTime) override;
	//Chase the harvester ship to get gold from it
	FVector ChaseShip(TArray<AActor*> Flock);
	// Initialize the DNA of the Predator ship
	virtual void SetDNA() override;
	// Calculate Predator Fitness
	virtual void CalculateAndStoreFitness(float alteration) override;
};
