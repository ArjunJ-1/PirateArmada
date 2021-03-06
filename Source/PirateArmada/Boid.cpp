// Fill out your copyright notice in the Description page of Project Settings.


#include "Boid.h"
#include "Components/SphereComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "ShipSpawner.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
ABoid::ABoid()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//setup boid mesh component & attach to root
	BoidMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Boid Mesh Component"));
	RootComponent = BoidMesh;
	BoidMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoidMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	
	//setup boid collision component and set as root
	BoidCollision = CreateDefaultSubobject<USphereComponent>(TEXT("Boid Collision Component"));
	BoidCollision->SetupAttachment(RootComponent);
	BoidCollision->SetCollisionObjectType(ECC_Pawn);
	BoidCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoidCollision->SetCollisionResponseToAllChannels(ECR_Overlap);
	BoidCollision->SetSphereRadius(30.0f);
	
	//setup cohesion sensor component
	PerceptionSensor = CreateDefaultSubobject<USphereComponent>(TEXT("Perception Sensor Component"));
	PerceptionSensor->SetupAttachment(RootComponent);
	PerceptionSensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PerceptionSensor->SetCollisionResponseToAllChannels(ECR_Ignore);
	PerceptionSensor->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PerceptionSensor->SetSphereRadius(300.0f);

	//set default velocity
	BoidVelocity = FVector::ZeroVector;

	//empty sensor array
	AvoidanceSensors.Empty();

	//theta angle of rotation on xy plane around z axis (yaw) around sphere

	//direction vector pointing from the center to point on sphere surface
	FVector SensorDirection;

	for (int32 i = 0; i < NumSensors; ++i)
	{
		//calculate the spherical coordinates of the direction vectors endpoint
		float yaw = 2 * UKismetMathLibrary::GetPI() * GoldenRatio * i;
		float pitch = FMath::Acos(1 - (2 * float(i) / NumSensors));

		//convert point on sphere to cartesian coordinates xyz
		SensorDirection.X = FMath::Cos(yaw)*FMath::Sin(pitch);
		SensorDirection.Y = FMath::Sin(yaw)*FMath::Sin(pitch);
		SensorDirection.Z = FMath::Cos(pitch);
		//add direction to list of sensors for avoidance
		AvoidanceSensors.Emplace(SensorDirection);
	}
}

// Called when the game starts or when spawned
void ABoid::BeginPlay()
{
	Super::BeginPlay();
	//set velocity based on spawn rotation and flock speed settings
	BoidVelocity = this->GetActorForwardVector();
	BoidVelocity.Normalize();
	BoidVelocity *= FMath::FRandRange(MinSpeed, MaxSpeed);

	BoidCollision->OnComponentBeginOverlap.AddDynamic(this, &ABoid::OnHitboxOverlapBegin);
	BoidCollision->OnComponentEndOverlap.AddDynamic(this, &ABoid::OnHitboxOverlapEnd);
	
	//set current mesh rotation
	CurrentRotation = this->GetActorRotation();
}

// Called every frame
void ABoid::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Incentive on being alive
	TimeAlive += DeltaTime;
	CalculateAndStoreFitness(DeltaTime * 10);
	//move ship
	FlightPath(DeltaTime);
	
	//update ship mesh rotation
	UpdateMeshRotation();

	if(CollisionCloud != nullptr)
	{
		GoldCollected += CollisionCloud->RemoveGold();
		// Incentive to collect gold
		CalculateAndStoreFitness(1);
	}

	if(Invincibility > 0)
	{
		Invincibility -= DeltaTime;
	}
}

void ABoid::UpdateMeshRotation()
{
	//rotate toward current boid heading smoothly
	CurrentRotation = FMath::RInterpTo(CurrentRotation, this->GetActorRotation(), GetWorld()->DeltaTimeSeconds, 7.0f);
	this->BoidMesh->SetWorldRotation(CurrentRotation);
}

//-------------------------------------Lab 8 Starts Here---------------------------------------------------------------

// Predicate to overload operator () to compare heap values


void ABoid::FlightPath(float DeltaTime)
{
	FVector Acceleration = FVector::ZeroVector;

	// update position and rotation
	SetActorLocation(GetActorLocation() + (BoidVelocity * DeltaTime));
	SetActorRotation(BoidVelocity.ToOrientationQuat());

	// Get all nearby harvesters
	TArray<AActor*> NearbyShips;
	PerceptionSensor->GetOverlappingActors(NearbyShips, TSubclassOf<ABoid>());
	// Get all nearby pirates
	TArray<AActor*> NearbyPirates;
	PerceptionSensor->GetOverlappingActors(NearbyShips, TSubclassOf<APirate>());

	// Punish the Boids for being alone
	if(NearbyShips.Num() == 0)
		CalculateAndStoreFitness(-DeltaTime);
	
	Acceleration += AvoidShips(NearbyShips);
	Acceleration += VelocityMatching(NearbyShips);
	Acceleration += FlockCentering(NearbyShips);
	// Accelerate away from the pirates
	Acceleration += AvoidPredator(NearbyPirates);
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
	BoidVelocity = BoidVelocity.GetClampedToSize(MinSpeed, MaxSpeed);
}

FVector ABoid::AvoidShips(TArray<AActor*> NearbyShips)
{
	FVector Steering = FVector::ZeroVector;
	int ShipCount = 0;
	FVector SeparationDirection = FVector::ZeroVector;
	float ProximityFactor = 0.0f;

	for(AActor* OverlapActor : NearbyShips)
	{
		ABoid* LocalShip = Cast<ABoid>(OverlapActor);
		if(LocalShip != nullptr && LocalShip != this)
		{
			// check if the LocalShip is outside perception fov
			if(FVector::DotProduct(GetActorForwardVector(),
				(LocalShip->GetActorLocation() - GetActorLocation()).GetSafeNormal()) <= SeparationFOV)
			{
				continue; // local ship is outside the perception, disregard it and continue the loop
			}

			// get normalized direction form nearby ship
			SeparationDirection = GetActorLocation() - LocalShip->GetActorLocation();
			SeparationDirection = SeparationDirection.GetSafeNormal() * 2;

			// get scaling factor based off other ship's proximity. 0 = far away & 1 = very close
			ProximityFactor =  1.0f - (SeparationDirection.Size() / PerceptionSensor->GetScaledSphereRadius());

			// add steering force of ship and increase flock count
			Steering += (ProximityFactor * SeparationDirection);
			ShipCount++;
		}
	}

	if(ShipCount > 0)
	{
		// get flock average separation force, apply separation steering strength factor and return force
		Steering /= ShipCount;
		Steering.GetSafeNormal() -= BoidVelocity.GetSafeNormal();
		Steering *= SeparationStrength;
		return Steering;
	}
	return FVector::ZeroVector;
}

FVector ABoid::VelocityMatching(TArray<AActor*> NearbyShips)
{
	FVector Steering = FVector::ZeroVector;
	int ShipCount = 0;

	for (AActor* OverlapActor : NearbyShips)
	{
		ABoid* LocalShip = Cast<ABoid>(OverlapActor);
		if(LocalShip != nullptr && LocalShip != this)
		{
			if(FVector::DotProduct(GetActorForwardVector(),
				(LocalShip->GetActorLocation() - GetActorLocation()).GetSafeNormal()) <= AlignmentFOV)
			{
				continue; //LocalShip is outside viewing angle, 
			}
			// add LocalShip's alignment force
			Steering += LocalShip->BoidVelocity.GetSafeNormal();
			ShipCount++;
		}
	}

	if(ShipCount > 0)
	{
		// get steering force to average flocking direction
		Steering /= ShipCount;
		Steering.GetSafeNormal() -= BoidVelocity.GetSafeNormal();
		Steering *= VelocityStrength;
		return Steering;
	}
	return FVector::ZeroVector;
}

FVector ABoid::FlockCentering(TArray<AActor*> NearbyShips)
{
	FVector Steering = FVector::ZeroVector;
	int ShipCount = 0;
	FVector AveragePosition = FVector::ZeroVector;

	for (AActor* OverlapActor : NearbyShips)
	{
		
		ABoid* LocalShip = Cast<ABoid>(OverlapActor);
		if(LocalShip != nullptr && LocalShip != this)
		{
			if(FVector::DotProduct(GetActorForwardVector(),
				(LocalShip->GetActorLocation() - GetActorLocation()).GetSafeNormal()) <= CohesionFOV)
			{
				continue; //LocalShip is outside viewing angle, 
			}

			//get cohesive force to group with LocalShip
			AveragePosition += LocalShip->GetActorLocation();
			ShipCount++;
		}
	}

	if(ShipCount > 0)
	{
		// average cohesion force of ships
		AveragePosition /= ShipCount;
		Steering = AveragePosition - GetActorLocation();
		Steering.GetSafeNormal() -= BoidVelocity.GetSafeNormal();
		Steering *= CenteringStrength;
		return Steering;
	}
	return FVector::ZeroVector;
}

FVector ABoid::AvoidObstacle()
{
	FVector Steering = FVector::ZeroVector;
	FQuat SensorRotation = FQuat::FindBetweenVectors(AvoidanceSensors[0], GetActorForwardVector());
	FVector NewSensorDirection = FVector::ZeroVector;
	FCollisionQueryParams TraceParams;
	FHitResult OutHit;

	for(FVector AvoidanceSensor : AvoidanceSensors)
	{
		//Trace Check
		NewSensorDirection = SensorRotation.RotateVector(AvoidanceSensor);
		GetWorld()->LineTraceSingleByChannel(OutHit,
			GetActorLocation(),
			GetActorLocation() + NewSensorDirection * SensorRadius,
			ECC_GameTraceChannel1,
			TraceParams);

		if(!OutHit.bBlockingHit)
		{
			Steering = NewSensorDirection.GetSafeNormal() - BoidVelocity.GetSafeNormal();
			Steering *= AvoidanceStrength;
			return Steering;
		}
	}
	return FVector::ZeroVector;
}

// Avoid the pirate ship 
FVector ABoid::AvoidPredator(TArray<AActor*> NearbyPirates)
{
	FVector Steering = FVector::ZeroVector;
	int ShipCount = 0;
	FVector SeparationDirection = FVector::ZeroVector;
	float ProximityFactor = 0.0f;

	for(AActor* OverlapActor : NearbyPirates)
	{
		APirate* LocalShip = Cast<APirate>(OverlapActor);
		if(LocalShip != nullptr)
		{
			// check if the LocalShip is outside perception fov
			if(FVector::DotProduct(GetActorForwardVector(),
				(LocalShip->GetActorLocation() - GetActorLocation()).GetSafeNormal()) <= SeparationFOV)
			{
				continue; // local ship is outside the perception, disregard it and continue the loop
			}

			// get normalized direction form nearby Predator
			SeparationDirection = GetActorLocation() - LocalShip->GetActorLocation();
			SeparationDirection = SeparationDirection.GetSafeNormal() * 10;

			// add steering force of ship and increase flock count
			Steering += SeparationDirection;
			ShipCount++;
		}
	}

	if(ShipCount > 0)
	{
		// get flock average separation force, apply separation steering strength factor and return force
		Steering /= ShipCount;
		Steering.GetSafeNormal() -= BoidVelocity.GetSafeNormal();
		Steering *= RunAwayStrength;
		return Steering;
	}
	return FVector::ZeroVector;
}


//------------------------------------------Collision Code---------------------------------------------------------------------------


bool ABoid::IsObstacleAhead()
{
	if (AvoidanceSensors.Num() > 0)
	{
		FQuat SensorRotation = FQuat::FindBetweenVectors(AvoidanceSensors[0], this->GetActorForwardVector());
		FVector NewSensorDirection = FVector::ZeroVector;
		NewSensorDirection = SensorRotation.RotateVector(AvoidanceSensors[0]);
		FCollisionQueryParams TraceParameters;
		FHitResult Hit;
		//line trace
		GetWorld()->LineTraceSingleByChannel(Hit,
			this->GetActorLocation(),
			this->GetActorLocation() + NewSensorDirection * SensorRadius,
			ECC_GameTraceChannel1,
			TraceParameters);

		//check if boid is inside object (i.e. no need to avoid/impossible to)
		if (Hit.bBlockingHit)
		{
			TArray<AActor*> OverlapActors;
			BoidCollision->GetOverlappingActors(OverlapActors);
			for (AActor* OverlapActor : OverlapActors)
			{
				if (Hit.Actor == OverlapActor)
				{
					return false;
				}
			}
		}
		return Hit.bBlockingHit;
	}
	return false;
}

void ABoid::OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor != this)
	{
		if(Invincibility <= 0)
		{
			AGasCloud* cloud = Cast<AGasCloud>(OtherActor);
			if (cloud != nullptr)
			{
				CollisionCloud = cloud;
				return;
			}

			// Pirate and Harvester Collide
			APirate* PirateShip = Cast<APirate>(OtherActor);
			if (PirateShip != nullptr)
			{
				CalculateAndStoreFitness(-(0.50 * shipDNA.m_storedfitness));     // 50% reduction
				Cast<APirate>(PirateShip)->TimeOut = 5.5;                               // Pause the predator ship
				Cast<APirate>(PirateShip)->GoldCollected = GoldCollected;               // Plunder the gold of this ship
				Cast<APirate>(PirateShip)->CalculateAndStoreFitness(10);        // 10 fitness for every ship destroyed

				Spawner->NumofShips--;
				Spawner->m_DeadDNA.Add(shipDNA);
				Destroy();
				return;
			}
			// Two Harvesters collide
			ABoid* Ship = Cast<ABoid>(OtherActor);
			if (Ship != nullptr)
			{
				Spawner->NumofShips--;
				CalculateAndStoreFitness(-(0.25 * shipDNA.m_storedfitness));// 25% reduction
				Spawner->m_DeadDNA.Add(shipDNA);
				Destroy();
				return;
			}
		}
		if(OtherActor->GetName().Contains("Cube") &&
			OverlappedComponent->GetName().Equals(TEXT("Boid Collision Component")))
		{
			//UE_LOG(LogTemp, Warning, TEXT("Collector Ship Collided With Wall"));
			Spawner->NumofShips--;
			Spawner->m_DeadDNA.Add(shipDNA);
			CalculateAndStoreFitness(-(0.25 * shipDNA.m_storedfitness)); // 25% reduction
			Destroy();
			
		}
	}
}

void ABoid::OnHitboxOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	AGasCloud* cloud = Cast<AGasCloud>(OtherActor);
	if (cloud != nullptr)
	{
		CollisionCloud = nullptr;
	}
}

// Set DNA of the harvester ships
void ABoid::SetDNA()
{
	VelocityStrength = shipDNA.StrengthValues[0];
	SeparationStrength = shipDNA.StrengthValues[1];
	CenteringStrength = shipDNA.StrengthValues[2];
	AvoidanceStrength = shipDNA.StrengthValues[3];
	GasCloudStrength = shipDNA.StrengthValues[4];
	SpeedStrength = shipDNA.StrengthValues[5];
	RunAwayStrength = shipDNA.StrengthValues[6];
}

// Calculate and store the fitness of the ship
void ABoid::CalculateAndStoreFitness(float alteration)
{
	if (shipDNA.m_storedfitness == -1)
	{
		//first time calculating fitness
		shipDNA.m_storedfitness = 0;
	}
	shipDNA.m_storedfitness += alteration;
}
