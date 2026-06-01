#include "PhysXVehicleInstance.h"

bool FPhysXVehicleInstance::IsValid() const
{
	return ChassisActor != nullptr && Vehicle != nullptr;
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