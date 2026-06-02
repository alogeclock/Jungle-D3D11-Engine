#include "PhysXVehicleInstance.h"

bool FPhysXVehicleInstance::IsValid() const
{
	return ChassisActor != nullptr && Vehicle != nullptr;
}

void FPhysXVehicleInstance::SetWheelVisualComponents(const TArray<USceneComponent*>& InWheelVisualComponents)
{
	WheelVisualComponents = InWheelVisualComponents;
	WheelVisualRotationOffsets.clear();
	WheelVisualRotationOffsets.reserve(WheelVisualComponents.size());

	for (USceneComponent* WheelComp : WheelVisualComponents)
	{
		WheelVisualRotationOffsets.push_back(WheelComp ? WheelComp->GetRelativeQuat() : FQuat::Identity);
	}
}

FQuat FPhysXVehicleInstance::GetWheelVisualRotationOffset(uint32 WheelIndex) const
{
	if (WheelIndex < WheelVisualRotationOffsets.size())
	{
		return WheelVisualRotationOffsets[WheelIndex];
	}

	return FQuat::Identity;
}

void  FPhysXVehicleInstance::Release()
{
	if (Vehicle)
	{
		Vehicle->free();
		Vehicle = nullptr;
	}
	if (ChassisActor)
	{
		ChassisActor->release();
		ChassisActor = nullptr;
	}
	if (BatchQuery)
	{
		BatchQuery->release();
		BatchQuery = nullptr;
	}
	if (FrictionPairs)
	{
		FrictionPairs->release();
		FrictionPairs = nullptr;
	}
}
