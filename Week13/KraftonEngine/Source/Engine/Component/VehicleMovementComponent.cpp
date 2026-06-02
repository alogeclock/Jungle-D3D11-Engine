#include "Component/VehicleMovementComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Debug/DrawDebugHelpers.h"
#include "GameFramework/AActor.h"
#include "Physics/Backends/PhysXPhysicsScene.h"
#include "Physics/Systems/Vehicle/PhysXVehicleInstance.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

void UVehicleMovementComponent::BeginPlay()
{
	UMovementComponent::BeginPlay();

	FPhysXPhysicsScene* PhysScene = GetPhysicsScene();
	if (!PhysScene) return;

	FVehicleRuntimeCreateDesc CreateDesc;
	CreateDesc.ChassisComponent = Cast<UPrimitiveComponent>(UpdatedComponent);
	if (!CreateDesc.ChassisComponent) return;

	CreateDesc.ChassisComponent->SetAutoCreatePhysicsBody(false);
	CreateDesc.WheelVisualComponents = WheelVisualComponents;
	BuildVehicleDesc(CreateDesc.BuildDesc);

	if (!CreateDesc.IsValid()) return;


	VehicleHandle = PhysScene->CreateVehicle(CreateDesc);
}

void UVehicleMovementComponent::EndPlay()
{
	FPhysXPhysicsScene* PhysScene = GetPhysicsScene();
	if (PhysScene && VehicleHandle.IsValid())
	{
		PhysScene->DestroyVehicle(VehicleHandle);
	}

	UMovementComponent::EndPlay();
}

void UVehicleMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UMovementComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// 이동은 PhysX vehicle update 결과만 신뢰
	// Tick에서 직접 transform 건드리지 않음
	if (bDrawDebug)
	{
		DrawDebugVehicle();
	}
}

void UVehicleMovementComponent::SetWheelVisualComponents(const TArray<USceneComponent*>& InWheelVisualComponents)
{
	WheelVisualComponents.clear();
	WheelVisualComponents.reserve(GMaxNumWheels);

	const uint32 NumWheelVisuals = static_cast<uint32>(InWheelVisualComponents.size());
	const uint32 NumToCopy = NumWheelVisuals < GMaxNumWheels ? NumWheelVisuals : GMaxNumWheels;
	for (uint32 i = 0; i < NumToCopy; ++i)
	{
		USceneComponent* WheelVisualComponent = InWheelVisualComponents[i];
		WheelVisualComponents.push_back(WheelVisualComponent);

		if (auto* WheelPrimitive = Cast<UPrimitiveComponent>(WheelVisualComponent))
		{
			WheelPrimitive->SetAutoCreatePhysicsBody(false);
		}
	}

	if (VehicleHandle.IsValid())
	{
		FPhysXVehicleInstance* Instance = static_cast<FPhysXVehicleInstance*>(VehicleHandle.NativeVehicle);
		if (Instance)
		{
			Instance->SetWheelVisualComponents(WheelVisualComponents);
		}
	}
}

void UVehicleMovementComponent::SetThrottle(float Value)
{
	if (!VehicleHandle.IsValid()) return;
	FPhysXVehicleInstance* Instance = static_cast<FPhysXVehicleInstance*>(VehicleHandle.NativeVehicle);
	if (Instance) Instance->InputState.Throttle = FMath::Clamp(Value, 0.f, 1.f);
}

void UVehicleMovementComponent::SetBrake(float Value)
{
	if (!VehicleHandle.IsValid()) return;
	FPhysXVehicleInstance* Instance = static_cast<FPhysXVehicleInstance*>(VehicleHandle.NativeVehicle);
	if (Instance) Instance->InputState.Brake = FMath::Clamp(Value, 0.f, 1.f);
}

void UVehicleMovementComponent::SetSteering(float Value)
{
	if (!VehicleHandle.IsValid()) return;
	FPhysXVehicleInstance* Instance = static_cast<FPhysXVehicleInstance*>(VehicleHandle.NativeVehicle);
	if (Instance) Instance->InputState.Steering = FMath::Clamp(Value, -1.f, 1.f);
}

void UVehicleMovementComponent::SetHandbrake(bool bEnabled)
{
	if (!VehicleHandle.IsValid()) return;
	FPhysXVehicleInstance* Instance = static_cast<FPhysXVehicleInstance*>(VehicleHandle.NativeVehicle);
	if (Instance) Instance->InputState.bHandbrake = bEnabled;
}

float UVehicleMovementComponent::GetForwardSpeedKmh() const
{
	if (!VehicleHandle.IsValid()) return 0.f;
	FPhysXVehicleInstance* Instance = static_cast<FPhysXVehicleInstance*>(VehicleHandle.NativeVehicle);
	if (!Instance || !Instance->Vehicle) return 0.f;

	return Instance->Vehicle->computeForwardSpeed() * 3.6f;
}

float UVehicleMovementComponent::GetEngineRpm() const
{
	if (!VehicleHandle.IsValid()) return 0.f;
	FPhysXVehicleInstance* Instance = static_cast<FPhysXVehicleInstance*>(VehicleHandle.NativeVehicle);
	if (!Instance || !Instance->Vehicle) return 0.f;

	return Instance->Vehicle->mDriveDynData.getEngineRotationSpeed() * 60.0f / 6.28318530718f;
}

int32 UVehicleMovementComponent::GetCurrentGear() const
{
	if (!VehicleHandle.IsValid()) return 0;
	FPhysXVehicleInstance* Instance = static_cast<FPhysXVehicleInstance*>(VehicleHandle.NativeVehicle);
	if (!Instance || !Instance->Vehicle) return 0;
	return (int32)Instance->Vehicle->mDriveDynData.getCurrentGear();
}

bool UVehicleMovementComponent::IsInAir() const
{
	if (!VehicleHandle.IsValid()) return false;
	FPhysXVehicleInstance* Instance = static_cast<FPhysXVehicleInstance*>(VehicleHandle.NativeVehicle);
	if (!Instance) return false;

	// 모든 바퀴가 공중에 있으면 InAir
	for (uint32 i = 0; i < GMaxNumWheels; i++)
	{
		if (!Instance->WheelQueryResults[i].isInAir)
			return false;
	}
	return true;
}

const FVehicleRuntimeStats* UVehicleMovementComponent::GetRuntimeStats() const
{
	if (!VehicleHandle.IsValid()) return nullptr;
	FPhysXVehicleInstance* Instance = static_cast<FPhysXVehicleInstance*>(VehicleHandle.NativeVehicle);
	return Instance ? &Instance->DebugStats : nullptr;
}

FPhysXPhysicsScene* UVehicleMovementComponent::GetPhysicsScene() const
{
	AActor* Owner = GetOwner();
	if (!Owner) return nullptr;
	UWorld* World = Owner->GetWorld();
	if (!World) return nullptr;
	return static_cast<FPhysXPhysicsScene*>(World->GetPhysicsScene());
}

void UVehicleMovementComponent::BuildVehicleDesc(FVehicleBuildDesc& OutDesc) const
{
	FVehicleDriveDesc& Drive = OutDesc.DriveDesc;
	Drive.DriveType = DriveType;
	Drive.ChassisMass = ChassisMass;
	Drive.CenterOfMassOffset = CenterOfMassOffset;
	Drive.MaxEngineTorque = MaxEngineTorque;
	Drive.MaxEngineOmega = MaxEngineOmega;
	Drive.MaxBrakeTorque = MaxBrakeTorque;
	Drive.HandbrakeTorque = HandbrakeTorque;
	Drive.ClutchStrength = ClutchStrength;
	Drive.MaxSteerAngle = MaxSteerAngle;

	// WheelDescs는 AVehiclePawn에서 채워주거나
	// 여기서 4개 기본값으로 채움
	OutDesc.WheelDescs.clear();
	const float HalfWheelBase = WheelBase * 0.5f;
	const float HalfTrackWidth = TrackWidth * 0.5f;
	const EVehicleWheelRole FrontWheelRole =
		DriveType == EVehicleDriveType::VDT_RearWheel
		? EVehicleWheelRole::VWR_Steering
		: EVehicleWheelRole::VWR_SteerDrive;
	const EVehicleWheelRole RearWheelRole =
		DriveType == EVehicleDriveType::VDT_FrontWheel
		? EVehicleWheelRole::VWR_None
		: EVehicleWheelRole::VWR_Drive;

	const FVector WheelPositions[GMaxNumWheels] =
	{
		FVector( HalfWheelBase, -HalfTrackWidth, WheelZOffset),
		FVector( HalfWheelBase,  HalfTrackWidth, WheelZOffset),
		FVector(-HalfWheelBase, -HalfTrackWidth, WheelZOffset),
		FVector(-HalfWheelBase,  HalfTrackWidth, WheelZOffset),
	};
	const EVehicleWheelRole WheelRoles[GMaxNumWheels] =
	{
		FrontWheelRole,
		FrontWheelRole,
		RearWheelRole,
		RearWheelRole,
	};

	for (uint32 i = 0; i < GMaxNumWheels; i++)
	{
		FVehicleWheelDesc Wheel;
		Wheel.LocalPosition = WheelPositions[i];
		Wheel.WheelRole = WheelRoles[i];
		Wheel.Radius = WheelRadius;
		Wheel.Width = WheelWidth;
		Wheel.Mass = WheelMass;
		Wheel.Suspension.Stiffness = SuspensionStiffness;
		Wheel.Suspension.Damping = SuspensionDamping;
		Wheel.Suspension.MaxCompression = MaxCompression;
		Wheel.Suspension.MaxDroop = MaxDroop;
		OutDesc.WheelDescs.push_back(Wheel);
	}
}

void UVehicleMovementComponent::DrawDebugVehicle() const
{
	AActor* Owner = GetOwner();
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	const FVehicleRuntimeStats* Stats = GetRuntimeStats();
	if (!World || !Stats)
	{
		return;
	}

	DrawDebugSphere(World, Stats->CenterOfMassWorld, 0.8f, 8, FColor(255, 0, 255), 0.0f);

	for (const FVehicleWheelDebugState& Wheel : Stats->Wheels)
	{
		const FColor SuspensionColor = Wheel.bInAir ? FColor::Red() : FColor::Green();
		DrawDebugLine(World, Wheel.SuspensionStart, Wheel.SuspensionEnd, SuspensionColor, 0.0f);
		DrawDebugSphere(World, Wheel.WheelCenter, WheelRadius, 8, FColor(0, 200, 255), 0.0f);

		if (!Wheel.bInAir)
		{
			DrawDebugPoint(World, Wheel.ContactPoint, 0.08f, FColor::Yellow(), 0.0f);
			DrawDebugLine(World, Wheel.ContactPoint, Wheel.ContactPoint + Wheel.ContactNormal * 0.35f, FColor::Blue(), 0.0f);
		}
	}
}
