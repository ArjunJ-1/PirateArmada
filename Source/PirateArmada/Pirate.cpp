// Fill out your copyright notice in the Description page of Project Settings.


#include "Pirate.h"
#include "Components/SphereComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "DrawDebugHelpers.h"
#include "ShipSpawner.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
APirate::APirate()
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
void APirate::BeginPlay()
{
	Super::BeginPlay();
	//set velocity based on spawn rotation and flock speed settings
	BoidVelocity = this->GetActorForwardVector();
	BoidVelocity.Normalize();
	BoidVelocity *= FMath::FRandRange(MinSpeed, MaxSpeed);

	BoidCollision->OnComponentBeginOverlap.AddDynamic(this, &APirate::OnHitboxOverlapBegin);
	BoidCollision->OnComponentEndOverlap.AddDynamic(this, &APirate::OnHitboxOverlapEnd);
	
	//set current mesh rotation
	CurrentRotation = this->GetActorRotation();
}

// Called every frame
void APirate::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// Incentive on being alive
	TimeAlive += DeltaTime;
	CalculateAndStoreFitness(DeltaTime);
	//move ship
	FlightPath(DeltaTime);
	
	//update ship mesh rotation
	UpdateMeshRotation();

	if(Invincibility > 0)
	{
		Invincibility -= DeltaTime;
	}
}

void APirate::UpdateMeshRotation()
{
	//rotate toward current boid heading smoothly
	CurrentRotation = FMath::RInterpTo(CurrentRotation, this->GetActorRotation(), GetWorld()->DeltaTimeSeconds, 7.0f);
	this->BoidMesh->SetWorldRotation(CurrentRotation);
}

void APirate::FlightPath(float DeltaTime)
{
	// Pause the ship on timeout
	if(TimeOut > 0.5)
	{
		TimeOut -= DeltaTime;
		return;
	}
	
	FVector Acceleration = FVector::ZeroVector;

	// update position and rotation
	SetActorLocation(GetActorLocation() + (BoidVelocity * DeltaTime));
	SetActorRotation(BoidVelocity.ToOrientationQuat());

	// Get all the nearby pirates
	TArray<AActor*> NearbyShips;
	PerceptionSensor->GetOverlappingActors(NearbyShips, TSubclassOf<APirate>());
	// Get all the nearby Harvesters
	TArray<AActor*> NearbyHarvesters;
	PerceptionSensor->GetOverlappingActors(NearbyHarvesters, TSubclassOf<ABoid>());
	
	Acceleration += AvoidShips(NearbyShips);
	Acceleration += VelocityMatching(NearbyShips);
	Acceleration += FlockCentering(NearbyShips);
	Acceleration += ChaseShip(NearbyHarvesters);
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

// Avoid colliding with other pirates
FVector APirate::AvoidShips(TArray<AActor*> NearbyShips)
{
	FVector Steering = FVector::ZeroVector;
	int ShipCount = 0;
	FVector SeparationDirection = FVector::ZeroVector;
	float ProximityFactor = 0.0f;

	for(AActor* OverlapActor : NearbyShips)
	{
		APirate* LocalShip = Cast<APirate>(OverlapActor);
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

// Match the velocity of the pirate flock
FVector APirate::VelocityMatching(TArray<AActor*> NearbyShips)
{
	FVector Steering = FVector::ZeroVector;
	int ShipCount = 0;

	for (AActor* OverlapActor : NearbyShips)
	{
		APirate* LocalShip = Cast<APirate>(OverlapActor);
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

// Try to get to the center of the Pirate flock
FVector APirate::FlockCentering(TArray<AActor*> NearbyShips)
{
	FVector Steering = FVector::ZeroVector;
	int ShipCount = 0;
	FVector AveragePosition = FVector::ZeroVector;

	for (AActor* OverlapActor : NearbyShips)
	{
		
		APirate* LocalShip = Cast<APirate>(OverlapActor);
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

FVector APirate::AvoidObstacle()
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

// Chases the closest harvester ship
FVector APirate::ChaseShip(TArray<AActor*> NearbyShips)
{
	FVector Steering = FVector::ZeroVector;
	int ShipCount = 0;
	FVector SeparationDirection = FVector::ZeroVector;

	ABoid* TargetHarvester = nullptr;  // Target Harvester
	float NearestTargetDistance = 600; // 600 is the perception radius
	for(AActor* OverlapActor : NearbyShips)
	{
		ABoid* LocalShip = Cast<ABoid>(OverlapActor);
		if(LocalShip != nullptr)
		{
			// check if the LocalShip is outside perception fov
			if(FVector::DotProduct(GetActorForwardVector(),
				(LocalShip->GetActorLocation() - GetActorLocation()).GetSafeNormal()) <= ChaseFOV)
			{
				continue; // local ship is outside the perception, disregard it and continue the loop
			}
			// Find the closest ship
			if(FVector::Dist(LocalShip->GetActorLocation(), GetActorLocation()) < NearestTargetDistance)
			{
				// give incentive for getting close to the Harvesters (1 for every ship in view)
				CalculateAndStoreFitness(1);
				TargetHarvester = LocalShip;
				NearestTargetDistance = FVector::Dist(LocalShip->GetActorLocation(), GetActorLocation());
			}
		}
	}

	// If we find the Target harvester then steer towards it
	if (TargetHarvester != nullptr)
	{
		// add steering force of ship and increase flock count
		Steering += TargetHarvester->BoidVelocity.GetSafeNormal();
		ShipCount++;
	}
	
	if(ShipCount > 0)
	{
		// get flock average separation force, apply separation steering strength factor and return force
		Steering /= ShipCount;
		Steering.GetSafeNormal() -= BoidVelocity.GetSafeNormal();
		Steering *= ChaseStrength;
		return Steering;
	}
	return FVector::ZeroVector;
}

bool APirate::IsObstacleAhead()
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

void APirate::OnHitboxOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
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
		
			APirate* ship = Cast<APirate>(OtherActor);
			// Two Predators collide
			if (ship != nullptr)
			{
				Spawner->NumOfPredators--;
				CalculateAndStoreFitness(-(0.25 * shipDNA.m_storedfitness));// 25% reduction
				Spawner->m_DeadPredatorsDNA.Add(shipDNA);
				Destroy();
				return;
			}

		}
		// Predator Collides with wall
		if(OtherActor->GetName().Contains("Cube") &&
			OverlappedComponent->GetName().Equals(TEXT("Boid Collision Component")))
		{
			//UE_LOG(LogTemp, Warning, TEXT("Collector Ship Collided With Wall"));
			Spawner->NumOfPredators--;
			Spawner->m_DeadPredatorsDNA.Add(shipDNA);
			CalculateAndStoreFitness(-(0.25 * shipDNA.m_storedfitness)); // 25% reduction
			Destroy();
			
		}
	}
}

void APirate::OnHitboxOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex)
{
	AGasCloud* cloud = Cast<AGasCloud>(OtherActor);
	if (cloud != nullptr)
	{
		CollisionCloud = nullptr;
	}
}

// Pirate DNA Excludes SpeedStrength and includes ChaseStrength
void APirate::SetDNA()
{
	VelocityStrength = shipDNA.StrengthValues[0];
	SeparationStrength = shipDNA.StrengthValues[1];
	CenteringStrength = shipDNA.StrengthValues[2];
	AvoidanceStrength = shipDNA.StrengthValues[3];
	GasCloudStrength = shipDNA.StrengthValues[4];
	ChaseStrength = shipDNA.StrengthValues[5];
}

// Calculates the same way as Harvesters (Boids)
void APirate::CalculateAndStoreFitness(float alteration)
{
	if (shipDNA.m_storedfitness == -1)
	{
		//first time calculating fitness
		shipDNA.m_storedfitness = 0;
	}
	shipDNA.m_storedfitness += alteration;
}

