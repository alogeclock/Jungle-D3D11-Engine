#pragma once

#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "VehicleInputTypes.h"

struct FVehicleWheelDebugState
{
	FVector SuspensionStart = FVector::ZeroVector;
	FVector SuspensionEnd = FVector::ZeroVector;
	FVector ContactPoint = FVector::ZeroVector;
	FVector ContactNormal = FVector::ZeroVector;
	FVector WheelCenter = FVector::ZeroVector;
	bool bInAir = true;
};

struct FVehicleDebugDrawOptions
{
	bool bDrawWheel = true;
	bool bDrawSuspension = true;
	bool bDrawContact = true;
	bool bDrawDriveForce = false;
};

struct FVehicleRuntimeStats
{
	int32 WheelCount = 0;
	float CurrentSpeed = 0.0f;
	float EngineRpm = 0.0f;
	float EngineTorque = 0.0f;
	float BrakeTorque = 0.0f;
	float SteeringAngle = 0.0f;
	bool bInAir = true;
	FVehicleInputState InputState;
	FVector CenterOfMassWorld = FVector::ZeroVector;
	TArray<FVehicleWheelDebugState> Wheels;
};
