#pragma once
#include "Component/Movement/MovementComponent.h"
#include "Physics/Systems/Vehicle/FVehicleRuntimeTypes.h"
#include "Physics/Systems/Vehicle/VehicleDebugTypes.h"
#include "Physics/Systems/Vehicle/VehicleInputTypes.h"
#include "Physics/Systems/Vehicle/VehicleTypes.h"
#include "VehicleMovementComponent.generated.h"

class USceneComponent;
class FPhysXPhysicsScene;

UCLASS()
class UVehicleMovementComponent : public UMovementComponent
{
public:
	GENERATED_BODY(UVehicleMovementComponent)

	UVehicleMovementComponent() = default;
	~UVehicleMovementComponent() override = default;

	void BeginPlay() override;
	void EndPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void SetWheelVisualComponents(const TArray<USceneComponent*>& InWheelVisualComponents);

	// Input API
	void SetThrottle(float Value);
	void SetBrake(float Value);
	void SetSteering(float Value);
	void SetHandbrake(bool bEnabled);

	// State Query
	float GetForwardSpeedKmh() const;
	float GetEngineRpm() const;
	int32 GetCurrentGear() const;
	bool  IsInAir() const;
	const FVehicleRuntimeStats* GetRuntimeStats() const;

	bool IsVehicleValid() const { return VehicleHandle.IsValid(); }

protected:
	FPhysXPhysicsScene* GetPhysicsScene() const;
	void BuildVehicleDesc(FVehicleBuildDesc& OutDesc) const;
	void DrawDebugVehicle() const;

private:
	FVehicleRuntimeHandle VehicleHandle;
	TArray<USceneComponent*> WheelVisualComponents;

	// Tuning - 에디터에서 편집 가능
	UPROPERTY(Edit, Category = "Vehicle|Drive", DisplayName = "Drive Type", Type=Enum, Enum=StaticEnum_EVehicleDriveType())
	EVehicleDriveType DriveType = EVehicleDriveType::VDT_FourWheel;

	UPROPERTY(Edit, Category = "Vehicle|Drive", DisplayName = "Chassis Mass")
	float ChassisMass = 1200.f;

	UPROPERTY(Edit, Category = "Vehicle|Drive", DisplayName = "Center Of Mass Offset")
	FVector CenterOfMassOffset = FVector(0.0f, 0.0f, -0.25f);

	UPROPERTY(Edit, Category = "Vehicle|Drive", DisplayName = "Max Engine Torque")
	float MaxEngineTorque = 1500.f;

	UPROPERTY(Edit, Category = "Vehicle|Drive", DisplayName = "Max Brake Torque")
	float MaxBrakeTorque = 3000.f;

	UPROPERTY(Edit, Category = "Vehicle|Drive", DisplayName = "Handbrake Torque")
	float HandbrakeTorque = 4000.f;

	UPROPERTY(Edit, Category = "Vehicle|Drive", DisplayName = "Clutch Strength")
	float ClutchStrength = 10.f;

	UPROPERTY(Edit, Category = "Vehicle|Drive", DisplayName = "Max Steer Angle")
	float MaxSteerAngle = 35.f;

	UPROPERTY(Edit, Category = "Vehicle|Suspension", DisplayName = "Suspension Stiffness")
	float SuspensionStiffness = 35000.f;

	UPROPERTY(Edit, Category = "Vehicle|Suspension", DisplayName = "Suspension Damping")
	float SuspensionDamping = 4500.f;

	UPROPERTY(Edit, Category = "Vehicle|Suspension", DisplayName = "Max Compression")
	float MaxCompression = 0.3f;

	UPROPERTY(Edit, Category = "Vehicle|Suspension", DisplayName = "Max Droop")
	float MaxDroop = 0.1f;

	UPROPERTY(Edit, Category = "Vehicle|Wheel", DisplayName = "Wheel Radius")
	float WheelRadius = 0.3f;

	UPROPERTY(Edit, Category = "Vehicle|Wheel", DisplayName = "Wheel Width")
	float WheelWidth = 0.2f;

	UPROPERTY(Edit, Category = "Vehicle|Wheel", DisplayName = "Wheel Mass")
	float WheelMass = 20.f;

	UPROPERTY(Edit, Category = "Vehicle|Wheel", DisplayName = "Wheel Base")
	float WheelBase = 2.6f;

	UPROPERTY(Edit, Category = "Vehicle|Wheel", DisplayName = "Track Width")
	float TrackWidth = 1.6f;

	UPROPERTY(Edit, Category = "Vehicle|Wheel", DisplayName = "Wheel Z Offset")
	float WheelZOffset = -0.35f;

	UPROPERTY(Edit, Category = "Vehicle|Debug", DisplayName = "Draw Vehicle Debug")
	bool bDrawDebug = false;
};
