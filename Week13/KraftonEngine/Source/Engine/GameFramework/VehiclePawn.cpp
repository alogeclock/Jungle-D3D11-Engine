#include "GameFramework/VehiclePawn.h"

#include "Component/BoxComponent.h"
#include "Component/CameraComponent.h"
#include "Component/SpringArmComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/VehicleMovementComponent.h"
#include "Core/CollisionTypes.h"
#include "Engine/Runtime/Engine.h"
#include "Input/InputSystem.h"
#include "Mesh/MeshManager.h"

namespace
{
	const FVector VehicleChassisExtent(1.2f, 0.6f, 0.35f);
	const FVector VehicleSpawnRootOffset(0.0f, 0.0f, 1.0f);
	const FVector VehicleWheelFrontLeft(1.3f, -0.8f, -0.35f);
	const FVector VehicleWheelFrontRight(1.3f, 0.8f, -0.35f);
	const FVector VehicleWheelRearLeft(-1.3f, -0.8f, -0.35f);
	const FVector VehicleWheelRearRight(-1.3f, 0.8f, -0.35f);

	UStaticMesh* LoadVehiclePreviewMesh(const FString& Path)
	{
		if (!GEngine)
		{
			return nullptr;
		}

		ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
		return FMeshManager::LoadStaticMesh(Path, Device);
	}
}

void AVehiclePawn::BeginPlay()
{
	if (!VehicleMovement)
	{
		InitDefaultComponents();
	}

	APawn::BeginPlay();
}

void AVehiclePawn::Tick(float DeltaTime)
{
	APawn::Tick(DeltaTime);
}

void AVehiclePawn::PossessedBy(APlayerController* PC)
{
	APawn::PossessedBy(PC);
	ResetVehicleInput();
}

void AVehiclePawn::UnPossessed()
{
	ResetVehicleInput();
	APawn::UnPossessed();
}

void AVehiclePawn::ProcessPlayerInput(const FInputSystemSnapshot& Input, float DeltaTime)
{
	APawn::ProcessPlayerInput(Input, DeltaTime);

	if (bUseDefaultVehicleInput && IsPossessed())
	{
		ApplyDefaultVehicleInput(Input);
	}
}

void AVehiclePawn::InitDefaultComponents()
{
	if (VehicleMovement)
	{
		return;
	}

	ChassisComponent = AddComponent<UBoxComponent>();
	ChassisComponent->SetFName(FName("VehicleChassis"));
	ChassisComponent->SetBoxExtent(VehicleChassisExtent);
	ChassisComponent->SetRelativeLocation(VehicleSpawnRootOffset);
	ChassisComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ChassisComponent->SetCollisionObjectType(ECollisionChannel::ECC_PhysicsBody);
	ChassisComponent->SetSimulatePhysics(true);
	ChassisComponent->SetAutoCreatePhysicsBody(false);
	SetRootComponent(ChassisComponent);

	UStaticMesh* WheelMesh = LoadVehiclePreviewMesh("Asset/Mesh/BasicShape/Cylinder_StaticMesh.uasset");

	WheelFrontLeft = AddComponent<UStaticMeshComponent>();
	WheelFrontLeft->SetFName(FName("WheelFrontLeft"));
	WheelFrontLeft->SetStaticMesh(WheelMesh);
	WheelFrontLeft->SetRelativeLocation(VehicleWheelFrontLeft);
	WheelFrontLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WheelFrontLeft->SetAutoCreatePhysicsBody(false);
	WheelFrontLeft->AttachToComponent(ChassisComponent);

	WheelFrontRight = AddComponent<UStaticMeshComponent>();
	WheelFrontRight->SetFName(FName("WheelFrontRight"));
	WheelFrontRight->SetStaticMesh(WheelMesh);
	WheelFrontRight->SetRelativeLocation(VehicleWheelFrontRight);
	WheelFrontRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WheelFrontRight->SetAutoCreatePhysicsBody(false);
	WheelFrontRight->AttachToComponent(ChassisComponent);

	WheelRearLeft = AddComponent<UStaticMeshComponent>();
	WheelRearLeft->SetFName(FName("WheelRearLeft"));
	WheelRearLeft->SetStaticMesh(WheelMesh);
	WheelRearLeft->SetRelativeLocation(VehicleWheelRearLeft);
	WheelRearLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WheelRearLeft->SetAutoCreatePhysicsBody(false);
	WheelRearLeft->AttachToComponent(ChassisComponent);

	WheelRearRight = AddComponent<UStaticMeshComponent>();
	WheelRearRight->SetFName(FName("WheelRearRight"));
	WheelRearRight->SetStaticMesh(WheelMesh);
	WheelRearRight->SetRelativeLocation(VehicleWheelRearRight);
	WheelRearRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WheelRearRight->SetAutoCreatePhysicsBody(false);
	WheelRearRight->AttachToComponent(ChassisComponent);

	SpringArm = AddComponent<USpringArmComponent>();
	SpringArm->SetFName(FName("VehicleSpringArm"));
	SpringArm->TargetArmLength = 5.0f;
	SpringArm->TargetOffset = FVector(-0.8f, 0.0f, 1.2f);
	SpringArm->AttachToComponent(ChassisComponent);

	Camera = AddComponent<UCameraComponent>();
	Camera->SetFName(FName("VehicleCamera"));
	Camera->SetRelativeRotation(FVector(0.0f, 10.0f, 0.0f));
	Camera->AttachToComponent(SpringArm);

	VehicleMovement = AddComponent<UVehicleMovementComponent>();
	VehicleMovement->SetFName(FName("VehicleMovementComponent"));
	VehicleMovement->SetUpdatedComponent(ChassisComponent);
	VehicleMovement->SetWheelVisualComponents({
		WheelFrontLeft,
		WheelFrontRight,
		WheelRearLeft,
		WheelRearRight
	});
}

void AVehiclePawn::PostDuplicate()
{
	ChassisComponent = Cast<UBoxComponent>(GetRootComponent());
	WheelFrontLeft = nullptr;
	WheelFrontRight = nullptr;
	WheelRearLeft = nullptr;
	WheelRearRight = nullptr;
	SpringArm = nullptr;
	Camera = nullptr;
	VehicleMovement = nullptr;

	for (UActorComponent* Component : GetComponents())
	{
		if (!Component)
		{
			continue;
		}

		const FString ComponentName = Component->GetName();
		if (ComponentName == "WheelFrontLeft")
		{
			WheelFrontLeft = Cast<UStaticMeshComponent>(Component);
		}
		else if (ComponentName == "WheelFrontRight")
		{
			WheelFrontRight = Cast<UStaticMeshComponent>(Component);
		}
		else if (ComponentName == "WheelRearLeft")
		{
			WheelRearLeft = Cast<UStaticMeshComponent>(Component);
		}
		else if (ComponentName == "WheelRearRight")
		{
			WheelRearRight = Cast<UStaticMeshComponent>(Component);
		}
		else if (ComponentName == "VehicleSpringArm")
		{
			SpringArm = Cast<USpringArmComponent>(Component);
		}
		else if (ComponentName == "VehicleCamera")
		{
			Camera = Cast<UCameraComponent>(Component);
		}
		else if (ComponentName == "VehicleMovementComponent")
		{
			VehicleMovement = Cast<UVehicleMovementComponent>(Component);
		}
	}

	if (!VehicleMovement)
	{
		VehicleMovement = GetComponentByClass<UVehicleMovementComponent>();
	}

	if (ChassisComponent && (!WheelFrontLeft || !WheelFrontRight || !WheelRearLeft || !WheelRearRight))
	{
		TArray<UStaticMeshComponent*> WheelCandidates;
		for (USceneComponent* Child : ChassisComponent->GetChildren())
		{
			if (UStaticMeshComponent* WheelCandidate = Cast<UStaticMeshComponent>(Child))
			{
				WheelCandidates.push_back(WheelCandidate);
			}
		}

		auto IsWheelAssigned = [&](UStaticMeshComponent* Candidate)
		{
			return Candidate
				&& (Candidate == WheelFrontLeft
					|| Candidate == WheelFrontRight
					|| Candidate == WheelRearLeft
					|| Candidate == WheelRearRight);
		};

		auto PickWheelByLocalQuadrant = [&](float ExpectedXSign, float ExpectedYSign)
		{
			UStaticMeshComponent* BestWheel = nullptr;
			float BestScore = 0.0f;
			bool bHasBest = false;

			for (UStaticMeshComponent* Candidate : WheelCandidates)
			{
				if (!Candidate || IsWheelAssigned(Candidate))
				{
					continue;
				}

				const FVector LocalPosition = Candidate->GetRelativeLocation();
				const float Score = LocalPosition.X * ExpectedXSign + LocalPosition.Y * ExpectedYSign;
				if (!bHasBest || Score > BestScore)
				{
					BestWheel = Candidate;
					BestScore = Score;
					bHasBest = true;
				}
			}

			return BestWheel;
		};

		if (!WheelFrontLeft)
		{
			WheelFrontLeft = PickWheelByLocalQuadrant(1.0f, -1.0f);
		}
		if (!WheelFrontRight)
		{
			WheelFrontRight = PickWheelByLocalQuadrant(1.0f, 1.0f);
		}
		if (!WheelRearLeft)
		{
			WheelRearLeft = PickWheelByLocalQuadrant(-1.0f, -1.0f);
		}
		if (!WheelRearRight)
		{
			WheelRearRight = PickWheelByLocalQuadrant(-1.0f, 1.0f);
		}
	}

	if (ChassisComponent)
	{
		ChassisComponent->SetAutoCreatePhysicsBody(false);
	}

	if (VehicleMovement && ChassisComponent)
	{
		VehicleMovement->SetUpdatedComponent(ChassisComponent);
		VehicleMovement->SetWheelVisualComponents({
			WheelFrontLeft,
			WheelFrontRight,
			WheelRearLeft,
			WheelRearRight
		});
	}
}

void AVehiclePawn::ApplyDefaultVehicleInput(const FInputSystemSnapshot& Input)
{
	if (!VehicleMovement)
	{
		return;
	}

	if (VehicleMovement->IsInAir())
	{
		ResetVehicleInput();
		return;
	}

	const float Throttle = Input.IsDown('W') ? 1.0f : 0.0f;
	const float Brake = Input.IsDown('S') ? 1.0f : 0.0f;
	const float SteerRight = Input.IsDown('D') ? 1.0f : 0.0f;
	const float SteerLeft = Input.IsDown('A') ? 1.0f : 0.0f;

	VehicleMovement->SetThrottle(Throttle);
	VehicleMovement->SetBrake(Brake);
	VehicleMovement->SetSteering(SteerRight - SteerLeft);
	VehicleMovement->SetHandbrake(Input.IsDown(VK_SPACE));
}

void AVehiclePawn::ResetVehicleInput()
{
	if (!VehicleMovement)
	{
		return;
	}

	VehicleMovement->SetThrottle(0.0f);
	VehicleMovement->SetBrake(0.0f);
	VehicleMovement->SetSteering(0.0f);
	VehicleMovement->SetHandbrake(false);
}
