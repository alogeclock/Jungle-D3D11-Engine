#pragma once
#include "VehicleInputTypes.h"

class UPrimitiveComponent;
class USceneComponent;

struct FVehicleRuntimeHandle
{
	void* NativeVehicle = nullptr;

	bool IsValid() const { return NativeVehicle != nullptr; }
	void Reset() { NativeVehicle = nullptr; }
};

struct FVehicleRuntimeCreateDesc
{
	UPrimitiveComponent* ChassisComponent = nullptr;
	TArray<USceneComponent*> WheelVisualComponents;
	FVehicleBuildDesc BuildDesc;

	bool IsValid() const
	{
		return ChassisComponent != nullptr && BuildDesc.IsValid();
	}
};