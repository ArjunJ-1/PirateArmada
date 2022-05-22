// Fill out your copyright notice in the Description page of Project Settings.


#include "Predator.h"

#include "ShipSpawner.h"

APredator::APredator()
{
	VelocityStrength = 550.f;
}

void APredator::BeginPlay()
{
	Super::BeginPlay();
}

void APredator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}

void APredator::FlightPath(float DeltaTime)
{
	Super::FlightPath(DeltaTime);
	// Pause the ship on timeout
	
	FVector Acceleration = FVector::ZeroVector;

	// update position and rotation
	SetActorLocation(GetActorLocation() + (BoidVelocity * DeltaTime));
	SetActorRotation(BoidVelocity.ToOrientationQuat());

	TArray<AActor*> NearbyShips;
	PerceptionSensor->GetOverlappingActors(NearbyShips, StaticClass());
	Acceleration += AvoidShips(NearbyShips);
	Acceleration += VelocityMatching(NearbyShips);
	Acceleration += FlockCentering(NearbyShips);
	if(IsObstacleAhead())
		Acceleration += AvoidObstacle();

	// apply gascloud forces
	for(int i = 0; i < Spawner->GasClouds.Num(); i++)
	{
		FVector Force = Spawner->GasClouds[i]->GetActorLocation() - GetActorLocation();
		if(Force.Size() < 1500)
			GasCloudForces.Add(Force);
	}


	for(FVector GasCloudForce : GasCloudForces)
	{
		GasCloudForce = GasCloudForce.GetSafeNormal() * GasCloudStrength;
		Acceleration += GasCloudForce;
		GasCloudForces.Remove(GasCloudForce);
	}

	//update velocity
	BoidVelocity += (Acceleration * DeltaTime);
	BoidVelocity = BoidVelocity.GetClampedToSize(MinSpeed, MaxSpeed + SpeedStrength/10000*300);
}

FVector APredator::ChaseShip(TArray<AActor*> Flock)
{
	return FVector::ZeroVector;
}

void APredator::SetDNA()
{
	VelocityStrength = shipDNA.StrengthValues[0];
	SeparationStrength = shipDNA.StrengthValues[1];
	CenteringStrength = shipDNA.StrengthValues[2];
	AvoidanceStrength = shipDNA.StrengthValues[3];
	GasCloudStrength = shipDNA.StrengthValues[4];
	ChaseStrength = shipDNA.StrengthValues[5];
}

void APredator::CalculateAndStoreFitness(float alteration)
{
	Super::CalculateAndStoreFitness(alteration);

	if (shipDNA.m_storedfitness == -1)
	{
		//first time calculating fitness
		shipDNA.m_storedfitness = 0;
	}
	shipDNA.m_storedfitness += alteration;
}
