#pragma once
#include "VehicleTypes.h"
#include "VehicleDebugTypes.h"
#include "PxPhysicsAPI.h"
#include "Component/SceneComponent.h"
#include "vehicle/PxVehicleSDK.h"
#include "vehicle/PxVehicleDrive4W.h"
#include "vehicle/PxVehicleUtilSetup.h"

using namespace physx;

static const PxU32 GMaxNumWheels = 4;

class UPrimitiveComponent;

class FPhysXVehicleInstance
{
public:
	// 생성/소멸
	FPhysXVehicleInstance() = default;
	~FPhysXVehicleInstance() { Release(); }

	// 복사 금지 (PhysX 포인터 때문에)
	FPhysXVehicleInstance(const FPhysXVehicleInstance&) = delete;
	FPhysXVehicleInstance& operator=(const FPhysXVehicleInstance&) = delete;

	bool IsValid() const;
	void Release();

	UPrimitiveComponent* ChassisComponent = nullptr;
	TArray<USceneComponent*> WheelVisualComponents;
	// PhysX 핵심 객체
	PxRigidDynamic* ChassisActor = nullptr;
	PxVehicleDrive4W* Vehicle = nullptr;
	PxBatchQuery* BatchQuery = nullptr;
	PxRaycastQueryResult                         SqResults[GMaxNumWheels];
	PxRaycastHit                                 SqHitBuffer[GMaxNumWheels];
	PxWheelQueryResult                           WheelQueryResults[GMaxNumWheels];
	PxVehicleWheelQueryResult                    VehicleQueryResult;
	PxVehicleDrivableSurfaceToTireFrictionPairs* FrictionPairs = nullptr;

	FVehicleInputState                           InputState;
	FVehicleRuntimeStats                         DebugStats;
};
