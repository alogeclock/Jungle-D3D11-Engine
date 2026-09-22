#pragma once

#include "GameFramework/Pawn.h"
#include "VehiclePawn.generated.h"

class UBoxComponent;
class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UVehicleMovementComponent;
struct FInputSystemSnapshot;

UCLASS(Actor)
class AVehiclePawn : public APawn
{
public:
	GENERATED_BODY(AVehiclePawn)

	AVehiclePawn() = default;
	~AVehiclePawn() override = default;

	void BeginPlay() override;
	void Tick(float DeltaTime) override;
	void PossessedBy(APlayerController* PC) override;
	void UnPossessed() override;
	void ProcessPlayerInput(const FInputSystemSnapshot& Input, float DeltaTime) override;
	void InitDefaultComponents();
	void PostDuplicate() override;

	UBoxComponent* GetChassisComponent() const { return ChassisComponent; }
	UVehicleMovementComponent* GetVehicleMovement() const { return VehicleMovement; }

private:
	void ApplyDefaultVehicleInput(const FInputSystemSnapshot& Input);
	void ResetVehicleInput();

	UPROPERTY(Edit, Category = "Vehicle|Input", DisplayName = "Use Default Vehicle Input")
	bool bUseDefaultVehicleInput = true;

	UBoxComponent* ChassisComponent = nullptr;
	UStaticMeshComponent* WheelFrontLeft = nullptr;
	UStaticMeshComponent* WheelFrontRight = nullptr;
	UStaticMeshComponent* WheelRearLeft = nullptr;
	UStaticMeshComponent* WheelRearRight = nullptr;
	USpringArmComponent* SpringArm = nullptr;
	UCameraComponent* Camera = nullptr;
	UVehicleMovementComponent* VehicleMovement = nullptr;
};
