#include "Physics/Backends/PhysXPhysicsScene.h"
#include "Physics/Backends/PhysXSceneThreading.h"
#include "Physics/Runtime/PhysicsConstraintInstance.h"
#include "Physics/Common/PhysicsConversionTypes.h"
#include "Physics/Systems/Vehicle/PhysXVehicleInstance.h"
#include "Physics/Systems/Vehicle/FVehicleRuntimeTypes.h"
#include "Physics/Systems/Vehicle/VehicleFilterConstants.h"
#include "Component/PrimitiveComponent.h"
#include "Component/BoxComponent.h"
#include "Component/SphereComponent.h"
#include "Component/CapsuleComponent.h"
#include "Component/ShapeComponent.h"
#include "Component/StaticMeshComponent.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Math/Quat.h"
#include "Object/Object.h"  // IsAliveObject
#include "Core/Log.h"

// PhysX headers
#include <PxPhysicsAPI.h>

#include <algorithm>
#include <cmath>
#include <utility>
#include <unordered_set>

using namespace physx;

static float PxVecLength(const PxVec3& V);

// ============================================================
// PhysX Error Callback
// ============================================================
class FPhysXErrorCallback : public PxErrorCallback
{
public:
	void reportError(PxErrorCode::Enum code, const char* message,
		const char* file, int line) override
	{
		const char* severity = "Info";
		if (code == PxErrorCode::eABORT || code == PxErrorCode::eOUT_OF_MEMORY)
			severity = "Fatal";
		else if (code == PxErrorCode::eINTERNAL_ERROR || code == PxErrorCode::eINVALID_OPERATION)
			severity = "Error";
		else if (code == PxErrorCode::eINVALID_PARAMETER || code == PxErrorCode::ePERF_WARNING)
			severity = "Warning";
		else if (code == PxErrorCode::eDEBUG_WARNING)
			severity = "Warning";
		UE_LOG("[PhysX %s] %s (%s:%d)", severity, message, file, line);
	}
};

static FPhysXErrorCallback GPhysXErrorCallback;
static PxDefaultAllocator  GPhysXAllocator;

physx::PxAllocatorCallback& GetSharedPhysXAllocator()
{
	return GPhysXAllocator;
}

physx::PxErrorCallback& GetSharedPhysXErrorCallback()
{
	return GPhysXErrorCallback;
}

// ============================================================
// PhysX Foundation/Physics 싱글턴
// ============================================================
static PxFoundation* GSharedFoundation = nullptr;
static PxPhysics*    GSharedPhysics    = nullptr;
static int32         GSharedRefCount   = 0;
static bool 		 GVehicleSdkInitialized = false;

static void AcquireSharedPhysX(PxFoundation*& OutFoundation, PxPhysics*& OutPhysics)
{
	if (GSharedRefCount == 0)
	{
		GSharedFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, GPhysXAllocator, GPhysXErrorCallback);
		if (GSharedFoundation)
			GSharedPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *GSharedFoundation, PxTolerancesScale());

		if (GSharedPhysics && PxInitVehicleSDK(*GSharedPhysics))
		{
			PxVehicleSetBasisVectors(PxVec3(0.0f, 0.0f, 1.0f), PxVec3(1.0f, 0.0f, 0.0f));
			PxVehicleSetUpdateMode(PxVehicleUpdateMode::eVELOCITY_CHANGE);
			GVehicleSdkInitialized = true;
		}
	}
	++GSharedRefCount;
	OutFoundation = GSharedFoundation;
	OutPhysics    = GSharedPhysics;
}

static void ReleaseSharedPhysX()
{
	if (--GSharedRefCount <= 0)
	{
		if (GVehicleSdkInitialized) { PxCloseVehicleSDK(); GVehicleSdkInitialized = false; }
		if (GSharedPhysics)    { GSharedPhysics->release();    GSharedPhysics    = nullptr; }
		if (GSharedFoundation) { GSharedFoundation->release(); GSharedFoundation = nullptr; }
		GSharedRefCount = 0;
	}
}

// ============================================================
// PhysX Simulation Event Callback
// ============================================================
class FPhysXSimulationCallback : public PxSimulationEventCallback
{
public:
	struct FQueuedHit
	{
		UPrimitiveComponent* Self  = nullptr;
		UPrimitiveComponent* Other = nullptr;
		FVector NormalImpulse{0,0,0};
		FHitResult Hit;
		bool bBegin = true;
	};
	struct FQueuedTrigger
	{
		UPrimitiveComponent* Self  = nullptr;
		UPrimitiveComponent* Other = nullptr;
		bool bBegin = true;
	};

	void onContact(const PxContactPairHeader& PairHeader,
		const PxContactPair* Pairs, PxU32 Count) override
	{
		if (PairHeader.flags & PxContactPairHeaderFlag::eREMOVED_ACTOR_0
			|| PairHeader.flags & PxContactPairHeaderFlag::eREMOVED_ACTOR_1)
			return;

		for (PxU32 i = 0; i < Count; ++i)
		{
			const PxContactPair& CP = Pairs[i];
			const bool bBegin   = CP.events.isSet(PxPairFlag::eNOTIFY_TOUCH_FOUND);
			const bool bPersist = CP.events.isSet(PxPairFlag::eNOTIFY_TOUCH_PERSISTS);
			const bool bEnd     = CP.events.isSet(PxPairFlag::eNOTIFY_TOUCH_LOST);
			if (!bBegin && !bPersist && !bEnd) continue;

			auto* CompA = CP.shapes[0] ? static_cast<UPrimitiveComponent*>(CP.shapes[0]->userData) : nullptr;
			auto* CompB = CP.shapes[1] ? static_cast<UPrimitiveComponent*>(CP.shapes[1]->userData) : nullptr;
			if (!CompA || !CompB) continue;

			PxContactPairPoint ContactPoints[16];
			PxU32 NumPoints = CP.extractContacts(ContactPoints, 16);
			for (PxU32 PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
			{
				const PxContactPairPoint& Point = ContactPoints[PointIndex];
				const float ImpulseMagnitude = PxVecLength(Point.impulse);
				if (Point.separation < -5.0f || ImpulseMagnitude > 100.0f)
				{
					const FString CompAName = CompA->GetName();
					const FString CompBName = CompB->GetName();
				}
			}

			if (bEnd)
			{
				PendingHits.push_back({ CompA, CompB, {}, {}, false });
				PendingHits.push_back({ CompB, CompA, {}, {}, false });
				continue;
			}

			if (!bBegin)
			{
				continue;
			}

			FVector ContactPos(0,0,0), ContactNormal(0,0,1);
			float Penetration = 0.0f;
			if (NumPoints > 0)
			{
				ContactPos    = FVector(ContactPoints[0].position.x, ContactPoints[0].position.y, ContactPoints[0].position.z);
				ContactNormal = FVector(ContactPoints[0].normal.x,   ContactPoints[0].normal.y,   ContactPoints[0].normal.z);
				Penetration   = ContactPoints[0].separation;
			}

			const FVector NI = ContactNormal * Penetration;

			FQueuedHit A;
			A.Self = CompA; A.Other = CompB; A.NormalImpulse = NI;
			A.Hit.bHit = true; A.Hit.HitComponent = CompB; A.Hit.HitActor = CompB->GetOwner();
			A.Hit.WorldHitLocation = ContactPos; A.Hit.ImpactNormal = ContactNormal;
			A.Hit.WorldNormal = ContactNormal; A.Hit.PenetrationDepth = -Penetration;
			PendingHits.push_back(A);

			FQueuedHit B;
			B.Self = CompB; B.Other = CompA; B.NormalImpulse = NI * -1.0f;
			B.Hit.bHit = true; B.Hit.HitComponent = CompA; B.Hit.HitActor = CompA->GetOwner();
			B.Hit.WorldHitLocation = ContactPos; B.Hit.ImpactNormal = ContactNormal * -1.0f;
			B.Hit.WorldNormal = ContactNormal * -1.0f; B.Hit.PenetrationDepth = -Penetration;
			PendingHits.push_back(B);
		}
	}

	void onTrigger(PxTriggerPair* Pairs, PxU32 Count) override
	{
		for (PxU32 i = 0; i < Count; ++i)
		{
			const PxTriggerPair& TP = Pairs[i];
			if (TP.flags & (PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER | PxTriggerPairFlag::eREMOVED_SHAPE_OTHER))
				continue;
			auto* TriggerComp = TP.triggerShape ? static_cast<UPrimitiveComponent*>(TP.triggerShape->userData) : nullptr;
			auto* OtherComp   = TP.otherShape   ? static_cast<UPrimitiveComponent*>(TP.otherShape->userData)   : nullptr;
			if (!TriggerComp || !OtherComp) continue;
			const bool bBegin = (TP.status == PxPairFlag::eNOTIFY_TOUCH_FOUND);
			const bool bEnd   = (TP.status == PxPairFlag::eNOTIFY_TOUCH_LOST);
			if (!bBegin && !bEnd) continue;
			if (TriggerComp->GetGenerateOverlapEvents()) PendingTriggers.push_back({ TriggerComp, OtherComp, bBegin });
			if (OtherComp->GetGenerateOverlapEvents())   PendingTriggers.push_back({ OtherComp, TriggerComp, bBegin });
		}
	}

	void DispatchPendingEvents()
	{
		std::vector<FQueuedHit>     Hits;    Hits.swap(PendingHits);
		std::vector<FQueuedTrigger> Trigs;   Trigs.swap(PendingTriggers);

		for (FQueuedHit& E : Hits)
		{
			if (!IsAliveObject(E.Self) || !IsAliveObject(E.Other)) continue;
			AActor* OtherActor = E.Other->GetOwner();
			if (E.bBegin) E.Self->NotifyComponentHit(E.Self, OtherActor, E.Other, E.NormalImpulse, E.Hit);
			else          E.Self->NotifyComponentEndHit(E.Self, OtherActor, E.Other);
		}
		for (FQueuedTrigger& E : Trigs)
		{
			if (!IsAliveObject(E.Self) || !IsAliveObject(E.Other)) continue;
			AActor* OtherActor = E.Other->GetOwner();
			if (E.bBegin) { FHitResult D; E.Self->NotifyComponentBeginOverlap(E.Self, OtherActor, E.Other, 0, false, D); }
			else            E.Self->NotifyComponentEndOverlap(E.Self, OtherActor, E.Other, 0);
		}
	}

	void onConstraintBreak(PxConstraintInfo*, PxU32) override {}
	void onWake(PxActor**, PxU32) override {}
	void onSleep(PxActor**, PxU32) override {}
	void onAdvance(const PxRigidBody* const*, const PxTransform*, const PxU32) override {}

private:
	std::vector<FQueuedHit>     PendingHits;
	std::vector<FQueuedTrigger> PendingTriggers;
};

// ============================================================
// Transform 변환 유틸
// ============================================================
static PxVec3 ToPxVec3(const FVector& V) { return PxVec3(V.X, V.Y, V.Z); }
static PxQuat ToPxQuat(const FQuat& Q)   { return PxQuat(Q.X, Q.Y, Q.Z, Q.W); }
static FVector ToFVector(const PxVec3& V) { return FVector(V.x, V.y, V.z); }
static FQuat   ToFQuat  (const PxQuat& Q) { return FQuat(Q.x, Q.y, Q.z, Q.w); }
static float ToRadians(float Degrees) { return Degrees * (PxPi / 180.0f); }

static void SetComponentWorldRotation(USceneComponent* Comp, const FQuat& WorldRotation)
{
	if (!Comp) return;

	if (USceneComponent* Parent = Comp->GetParent())
	{
		const FQuat ParentWorldRotation = Parent->GetWorldMatrix().ToQuat();
		Comp->SetRelativeRotation(WorldRotation * ParentWorldRotation.Inverse());
		return;
	}

	Comp->SetRelativeRotation(WorldRotation);
}

static PxTransform GetPxTransform(UPrimitiveComponent* Comp)
{
	return PxTransform(ToPxVec3(Comp->GetWorldLocation()), ToPxQuat(Comp->GetWorldMatrix().ToQuat()));
}

static PxVehiclePadSmoothingData CreateVehiclePadSmoothingData()
{
	PxVehiclePadSmoothingData SmoothingData;
	for (PxU32 i = 0; i < PxVehicleDriveDynData::eMAX_NB_ANALOG_INPUTS; ++i)
	{
		SmoothingData.mRiseRates[i] = 6.0f;
		SmoothingData.mFallRates[i] = 10.0f;
	}

	SmoothingData.mRiseRates[PxVehicleDrive4WControl::eANALOG_INPUT_STEER_LEFT] = 2.5f;
	SmoothingData.mRiseRates[PxVehicleDrive4WControl::eANALOG_INPUT_STEER_RIGHT] = 2.5f;
	SmoothingData.mFallRates[PxVehicleDrive4WControl::eANALOG_INPUT_STEER_LEFT] = 5.0f;
	SmoothingData.mFallRates[PxVehicleDrive4WControl::eANALOG_INPUT_STEER_RIGHT] = 5.0f;

	return SmoothingData;
}

static const PxVehiclePadSmoothingData& GetVehiclePadSmoothingData()
{
	static const PxVehiclePadSmoothingData SmoothingData = CreateVehiclePadSmoothingData();
	return SmoothingData;
}

static const PxFixedSizeLookupTable<8>& GetVehicleSteerVsForwardSpeedTable()
{
	static const PxF32 SteerVsForwardSpeedData[] =
	{
		0.0f,        0.75f,
		5.0f,        0.75f,
		30.0f,       0.125f,
		120.0f,      0.1f,
	};
	static const PxFixedSizeLookupTable<8> SteerVsForwardSpeedTable(SteerVsForwardSpeedData, 4);
	return SteerVsForwardSpeedTable;
}

static PxTransform ToPxTransform(const FTransform& Transform)
{
	return PxTransform(ToPxVec3(Transform.Location), ToPxQuat(Transform.Rotation));
}

static FTransform ToFTransform(const PxTransform& Transform)
{
	return FTransform(ToFVector(Transform.p), ToFQuat(Transform.q), FVector::OneVector);
}

static float PxVecLength(const PxVec3& V)
{
	return std::sqrt(V.x * V.x + V.y * V.y + V.z * V.z);
}


static PxVec3 TransformPointToCloth(const FMatrix& WorldToCloth, const PxVec3& WorldPoint)
{
	const FVector P = WorldToCloth.TransformPositionWithW(FVector(WorldPoint.x, WorldPoint.y, WorldPoint.z));
	return PxVec3(P.X, P.Y, P.Z);
}

static PxVec3 TransformVectorToCloth(const FMatrix& WorldToCloth, const PxVec3& WorldVector)
{
	FVector V = WorldToCloth.TransformVector(FVector(WorldVector.x, WorldVector.y, WorldVector.z));
	if (!V.IsNearlyZero())
	{
		V.Normalize();
	}
	return PxVec3(V.X, V.Y, V.Z);
}

static float GetApproxUniformScaleToCloth(const FMatrix& WorldToCloth)
{
	// TODO: Non-uniform component scale should convert spheres/capsules to conservative shapes
	// or reject them. For now, use the largest transformed basis length to avoid underestimating radius.
	const float X = WorldToCloth.TransformVector(FVector(1.0f, 0.0f, 0.0f)).Length();
	const float Y = WorldToCloth.TransformVector(FVector(0.0f, 1.0f, 0.0f)).Length();
	const float Z = WorldToCloth.TransformVector(FVector(0.0f, 0.0f, 1.0f)).Length();
	return (std::max)(0.0001f, (std::max)(X, (std::max)(Y, Z)));
}

static bool ShouldGatherClothShape(
	const PxRigidActor* Actor,
	const PxShape* Shape,
	const FClothCollisionGatherParams& Params)
{
	if (!Actor || !Shape)
	{
		return false;
	}

	const PxShapeFlags ShapeFlags = Shape->getFlags();
	const bool bSimulationShape = ShapeFlags.isSet(PxShapeFlag::eSIMULATION_SHAPE);
	const bool bQueryShape = ShapeFlags.isSet(PxShapeFlag::eSCENE_QUERY_SHAPE);
	const bool bTriggerShape = ShapeFlags.isSet(PxShapeFlag::eTRIGGER_SHAPE);
	if (!bSimulationShape && (!bQueryShape || bTriggerShape))
	{
		return false;
	}

	const bool bStatic = Actor->is<PxRigidStatic>() != nullptr;
	const bool bDynamic = Actor->is<PxRigidDynamic>() != nullptr;
	if ((bStatic && !Params.bIncludeStatic) || (bDynamic && !Params.bIncludeDynamic))
	{
		return false;
	}

	UPrimitiveComponent* ShapeComp = static_cast<UPrimitiveComponent*>(Shape->userData);
	if (ShapeComp && ShapeComp == Params.IgnoreComponent)
	{
		return false;
	}

	AActor* ShapeActor = ShapeComp ? ShapeComp->GetOwner() : static_cast<AActor*>(Actor->userData);
	if (ShapeActor && ShapeActor == Params.IgnoreActor)
	{
		return false;
	}

	return true;
}

static void AddClothPlaneFromWorld(
	const FMatrix& WorldToCloth,
	const PxVec3& WorldPoint,
	const PxVec3& WorldNormal,
	float Thickness,
	FClothCollisionData& Out)
{
	const PxVec3 LocalPoint = TransformPointToCloth(WorldToCloth, WorldPoint);
	PxVec3 LocalNormal = TransformVectorToCloth(WorldToCloth, WorldNormal);
	const float LenSq = LocalNormal.x * LocalNormal.x + LocalNormal.y * LocalNormal.y + LocalNormal.z * LocalNormal.z;
	if (LenSq <= 1.0e-8f)
	{
		return;
	}

	// NvCloth plane convention is assumed as dot(n, x) + d = 0.
	// TODO: Verify box plane normal sign with one-sided cloth collision tests.
	// Move the plane inward by the cloth collision thickness to create separation
	// from box/plane-like rigid geometry.
	const float D = -(LocalNormal.x * LocalPoint.x + LocalNormal.y * LocalPoint.y + LocalNormal.z * LocalPoint.z) - Thickness;
	Out.Planes.push_back(PxVec4(LocalNormal.x, LocalNormal.y, LocalNormal.z, D));
}

static void GatherClothCollisionFromActor(
	const PxRigidActor* Actor,
	const FClothCollisionGatherParams& Params,
	FClothCollisionData& Out)
{
	if (!Actor)
	{
		return;
	}

	const PxU32 NumShapes = Actor->getNbShapes();
	if (NumShapes == 0)
	{
		return;
	}

	std::vector<PxShape*> Shapes(NumShapes);
	Actor->getShapes(Shapes.data(), NumShapes);

	const PxTransform ActorPose = Actor->getGlobalPose();
	const float RadiusScale = GetApproxUniformScaleToCloth(Params.WorldToCloth);
	const float CollisionThickness = (std::max)(0.0f, Params.Thickness) * RadiusScale;

	for (PxShape* Shape : Shapes)
	{
		if (!ShouldGatherClothShape(Actor, Shape, Params))
		{
			continue;
		}

		const PxTransform ShapePose = ActorPose * Shape->getLocalPose();
		switch (Shape->getGeometryType())
		{
		case PxGeometryType::eSPHERE:
		{
			if (Out.Spheres.size() >= Params.MaxSpheres)
			{
				continue;
			}

			PxSphereGeometry SphereGeom;
			if (!Shape->getSphereGeometry(SphereGeom))
			{
				continue;
			}

			const PxVec3 LocalCenter = TransformPointToCloth(Params.WorldToCloth, ShapePose.p);
			Out.Spheres.push_back(PxVec4(LocalCenter.x, LocalCenter.y, LocalCenter.z, SphereGeom.radius * RadiusScale + CollisionThickness));
			break;
		}
		case PxGeometryType::eCAPSULE:
		{
			if (Out.Spheres.size() + 2 > Params.MaxSpheres)
			{
				continue;
			}

			PxCapsuleGeometry CapsuleGeom;
			if (!Shape->getCapsuleGeometry(CapsuleGeom))
			{
				continue;
			}

			// PhysX capsule local axis is X. The shape local pose already contains any engine-axis correction.
			const PxVec3 WorldAxis = ShapePose.q.rotate(PxVec3(1.0f, 0.0f, 0.0f));
			const PxVec3 WorldA = ShapePose.p + WorldAxis * CapsuleGeom.halfHeight;
			const PxVec3 WorldB = ShapePose.p - WorldAxis * CapsuleGeom.halfHeight;
			const PxVec3 LocalA = TransformPointToCloth(Params.WorldToCloth, WorldA);
			const PxVec3 LocalB = TransformPointToCloth(Params.WorldToCloth, WorldB);
			const uint32 SphereIndexA = static_cast<uint32>(Out.Spheres.size());
			const uint32 SphereIndexB = SphereIndexA + 1;
			const float Radius = CapsuleGeom.radius * RadiusScale + CollisionThickness;
			Out.Spheres.push_back(PxVec4(LocalA.x, LocalA.y, LocalA.z, Radius));
			Out.Spheres.push_back(PxVec4(LocalB.x, LocalB.y, LocalB.z, Radius));
			Out.Capsules.push_back(SphereIndexA);
			Out.Capsules.push_back(SphereIndexB);
			break;
		}
		case PxGeometryType::eBOX:
		{
			if (Out.Planes.size() + 6 > Params.MaxPlanes || Out.Planes.size() + 6 > 32)
			{
				continue;
			}

			PxBoxGeometry BoxGeom;
			if (!Shape->getBoxGeometry(BoxGeom))
			{
				continue;
			}

			const uint32 PlaneStart = static_cast<uint32>(Out.Planes.size());
			uint32 Mask = 0;
			const PxVec3 Axes[3] =
			{
				ShapePose.q.rotate(PxVec3(1.0f, 0.0f, 0.0f)),
				ShapePose.q.rotate(PxVec3(0.0f, 1.0f, 0.0f)),
				ShapePose.q.rotate(PxVec3(0.0f, 0.0f, 1.0f))
			};
			const float Extents[3] = { BoxGeom.halfExtents.x, BoxGeom.halfExtents.y, BoxGeom.halfExtents.z };

			for (uint32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
			{
				for (int32 SignIndex = 0; SignIndex < 2; ++SignIndex)
				{
					const float Sign = (SignIndex == 0) ? 1.0f : -1.0f;
					const PxVec3 WorldNormal = Axes[AxisIndex] * Sign;
					const PxVec3 WorldPoint = ShapePose.p + WorldNormal * Extents[AxisIndex];
					const uint32 PlaneIndex = static_cast<uint32>(Out.Planes.size());
					AddClothPlaneFromWorld(Params.WorldToCloth, WorldPoint, WorldNormal, CollisionThickness, Out);
					if (Out.Planes.size() > PlaneIndex)
					{
						Mask |= (1u << PlaneIndex);
					}
				}
			}

			if (Out.Planes.size() == PlaneStart + 6)
			{
				Out.ConvexMasks.push_back(Mask);
			}
			else
			{
				Out.Planes.resize(PlaneStart);
			}
			break;
		}
		default:
			break;
		}
	}
}

static const char* ToShapeTypeDebugName(EPhysicsShapeType ShapeType)
{
	switch (ShapeType)
	{
	case EPhysicsShapeType::PST_Sphere:  return "Sphere";
	case EPhysicsShapeType::PST_Box:     return "Box";
	case EPhysicsShapeType::PST_Capsule: return "Capsule";
	case EPhysicsShapeType::PST_Convex:  return "Convex";
	default:                             return "Unknown";
	}
}

static const char* ToCollisionEnabledDebugName(EPhysicsCollisionEnabled CollisionEnabled)
{
	switch (CollisionEnabled)
	{
	case EPhysicsCollisionEnabled::PCE_NoCollision:     return "NoCollision";
	case EPhysicsCollisionEnabled::PCE_QueryOnly:       return "QueryOnly";
	case EPhysicsCollisionEnabled::PCE_PhysicsOnly:     return "PhysicsOnly";
	case EPhysicsCollisionEnabled::PCE_QueryAndPhysics: return "QueryAndPhysics";
	default:                                            return "Unknown";
	}
}

static PxD6Motion::Enum ToPxD6Motion(EPhysicsConstraintMotionMode Motion)
{
	switch (Motion)
	{
	case EPhysicsConstraintMotionMode::Free:
		return PxD6Motion::eFREE;
	case EPhysicsConstraintMotionMode::Limited:
		return PxD6Motion::eLIMITED;
	case EPhysicsConstraintMotionMode::Locked:
	default:
		return PxD6Motion::eLOCKED;
	}
}

static void ApplyRootMassAndCOM(PxRigidDynamic* Dyn, UPrimitiveComponent* Root)
{
	if (!Dyn || !Root) return;
	const float MassKg = (Root->GetMass() > 0.0f) ? Root->GetMass() : 1.0f;
	PxRigidBodyExt::setMassAndUpdateInertia(*Dyn, MassKg);
	Dyn->setCMassLocalPose(PxTransform(ToPxVec3(Root->GetCenterOfMass())));
}

// ============================================================
// Collision Filtering
// ============================================================
static constexpr PxU32 FilterFlag_EnableOwnerSelfCollision = 1u << 31;

static void SetupFilterData(PxShape* Shape, UPrimitiveComponent* Comp, bool bEnableOwnerSelfCollision = false)
{
	PxFilterData Filter;
	if (!Shape || !Comp) return;

	Filter.word0 = static_cast<PxU32>(Comp->GetCollisionObjectType());
	Filter.word1 = 0; Filter.word2 = 0;
	Filter.word3 = Comp->GetOwner() ? Comp->GetOwner()->GetUUID() : 0;

	for (int32 Ch = 0; Ch < NumActiveCollisionChannels; ++Ch)
	{
		ECollisionResponse R = Comp->GetCollisionResponseToChannel(static_cast<ECollisionChannel>(Ch));
		if (R == ECollisionResponse::Block)   Filter.word1 |= (1u << Ch);
		if (R == ECollisionResponse::Overlap) Filter.word2 |= (1u << Ch);
	}
	if (bEnableOwnerSelfCollision)
	{
		Filter.word2 |= FilterFlag_EnableOwnerSelfCollision;
	}
	Shape->setSimulationFilterData(Filter);
	Shape->setQueryFilterData(Filter);
}

static bool IsVehicleDrivableSurface(UPrimitiveComponent* Comp)
{
	if (!Comp || !Comp->IsQueryCollisionEnabled()) return false;
	if (Comp->GetGenerateOverlapEvents()) return false;

	switch (Comp->GetCollisionObjectType())
	{
	case ECollisionChannel::ECC_WorldStatic:
		return true;
	default:
		return false;
	}
}

static void SetupVehicleQueryFilterData(PxShape* Shape, UPrimitiveComponent* Comp, bool bDrivable)
{
	if (!Shape) return;

	PxFilterData Filter = Shape->getQueryFilterData();
	Filter.word2 = Comp && Comp->GetOwner() ? Comp->GetOwner()->GetUUID() : 0;
	Filter.word3 = bDrivable ? DRIVABLE_SURFACE : UNDRIVABLE_SURFACE;
	Shape->setQueryFilterData(Filter);
}

static PxFilterFlags KraftonFilterShader(
	PxFilterObjectAttributes attributes0, PxFilterData filterData0,
	PxFilterObjectAttributes attributes1, PxFilterData filterData1,
	PxPairFlags& pairFlags, const void*, PxU32)
{
	const bool bSameOwner = filterData0.word3 != 0 && filterData0.word3 == filterData1.word3;
	const bool bAllowOwnerSelfCollision =
		(filterData0.word2 & filterData1.word2 & FilterFlag_EnableOwnerSelfCollision) != 0;
	if (bSameOwner && !bAllowOwnerSelfCollision)
	{
		return PxFilterFlag::eKILL;
	}

	if (PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1))
	{
		pairFlags = PxPairFlag::eTRIGGER_DEFAULT;
		return PxFilterFlag::eDEFAULT;
	}

	PxU32 channelA = filterData0.word0, channelB = filterData1.word0;
	bool bABlocksB = (filterData0.word1 & (1u << channelB)) != 0;
	bool bBBlocksA = (filterData1.word1 & (1u << channelA)) != 0;

	if (bABlocksB && bBBlocksA)
	{
		pairFlags = PxPairFlag::eCONTACT_DEFAULT
			| PxPairFlag::eNOTIFY_TOUCH_FOUND
			| PxPairFlag::eNOTIFY_TOUCH_PERSISTS
			| PxPairFlag::eNOTIFY_TOUCH_LOST
			| PxPairFlag::eNOTIFY_CONTACT_POINTS
			| PxPairFlag::eDETECT_CCD_CONTACT;
		return PxFilterFlag::eDEFAULT;
	}

	bool bAOverlapsB = (filterData0.word2 & (1u << channelB)) != 0;
	bool bBOverlapsA = (filterData1.word2 & (1u << channelA)) != 0;
	if (bAOverlapsB || bBOverlapsA)
	{
		pairFlags = PxPairFlag::eDETECT_DISCRETE_CONTACT
			| PxPairFlag::eNOTIFY_TOUCH_FOUND
			| PxPairFlag::eNOTIFY_TOUCH_LOST;
		return PxFilterFlag::eDEFAULT;
	}
	return PxFilterFlag::eKILL;
}

static void ConfigureShapeCollisionFlags(PxShape* Shape, const FPhysicsCollisionDesc& CollisionDesc)
{
	if (!Shape)
	{
		return;
	}

	const bool bQueryEnabled =
		CollisionDesc.CollisionEnabled == EPhysicsCollisionEnabled::PCE_QueryOnly ||
		CollisionDesc.CollisionEnabled == EPhysicsCollisionEnabled::PCE_QueryAndPhysics;
	const bool bPhysicsEnabled =
		CollisionDesc.CollisionEnabled == EPhysicsCollisionEnabled::PCE_PhysicsOnly ||
		CollisionDesc.CollisionEnabled == EPhysicsCollisionEnabled::PCE_QueryAndPhysics;

	Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, bQueryEnabled);
	Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, bPhysicsEnabled);
	Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, false);
}

static PxShape* CreateShapeFromDesc(
	PxPhysics* Physics,
	PxMaterial* DefaultMaterial,
	const FPhysicsShapeDesc& ShapeDesc,
	UPrimitiveComponent* OwnerComponent,
	bool bEnableOwnerSelfCollision = false)
{
	if (!Physics || !DefaultMaterial)
	{
		return nullptr;
	}

	PxGeometryHolder Geom;
	bool bHasGeometry = false;

	switch (ShapeDesc.ShapeType)
	{
	case EPhysicsShapeType::PST_Sphere:
		Geom = PxSphereGeometry((std::max)(0.01f, std::abs(ShapeDesc.Size.X)));
		bHasGeometry = true;
		break;
	case EPhysicsShapeType::PST_Box:
		Geom = PxBoxGeometry(
			(std::max)(0.01f, std::abs(ShapeDesc.Size.X)),
			(std::max)(0.01f, std::abs(ShapeDesc.Size.Y)),
			(std::max)(0.01f, std::abs(ShapeDesc.Size.Z)));
		bHasGeometry = true;
		break;
	case EPhysicsShapeType::PST_Capsule:
		Geom = PxCapsuleGeometry(
			(std::max)(0.01f, std::abs(ShapeDesc.Size.X)),
			(std::max)(0.01f, std::abs(ShapeDesc.Size.Y) * 0.5f));
		bHasGeometry = true;
		break;
	case EPhysicsShapeType::PST_Convex:
	default:
		break;
	}

	if (!bHasGeometry)
	{
		return nullptr;
	}

	PxShape* Shape = Physics->createShape(Geom.any(), *DefaultMaterial, true);
	if (!Shape)
	{
		return nullptr;
	}

	Shape->setLocalPose(ToPxTransform(ShapeDesc.LocalTransform));
	ConfigureShapeCollisionFlags(Shape, ShapeDesc.CollisionDesc);
	if (OwnerComponent)
	{
		SetupFilterData(Shape, OwnerComponent, bEnableOwnerSelfCollision);
	}

	Shape->userData = OwnerComponent;
	return Shape;
}

// ============================================================
// Lifecycle
// ============================================================

// PhysX SDK 객체와 multithreaded scene 설정을 초기화한다.
bool FPhysXPhysicsScene::InitializeScene(UWorld* InWorld, EPhysicsSceneType SceneType)
{
	World = InWorld;
	SetSceneType(SceneType);
	AcquireSharedPhysX(Foundation, Physics);
	if (!Foundation || !Physics)
	{
		UE_LOG("[PhysX] Failed to create Foundation or Physics");
		return false;
	}

	const uint32 WorkerThreadCount = GetRecommendedPhysXWorkerThreadCount();
	Dispatcher    = PxDefaultCpuDispatcherCreate(WorkerThreadCount);
	if (!Dispatcher)
	{
		UE_LOG("[PhysX] Failed to create CPU dispatcher");
		return false;
	}
	EventCallback = new FPhysXSimulationCallback();

	PxSceneDesc SceneDesc(Physics->getTolerancesScale());
	SceneDesc.gravity                  = PxVec3(0.0f, 0.0f, -9.81f);
	SceneDesc.cpuDispatcher            = Dispatcher;
	SceneDesc.filterShader             = KraftonFilterShader;
	SceneDesc.simulationEventCallback  = EventCallback;
	ApplyPhysXMultithreadedSceneSettings(SceneDesc);
	Scene = Physics->createScene(SceneDesc);
	if (!Scene)
	{
		UE_LOG("[PhysX] Failed to create Scene");
		return false;
	}
	SetNativeSceneHandle(Scene);
	DefaultMaterial = Physics->createMaterial(0.5f, 0.5f, 0.3f);
	InitializeClothScene();
	UE_LOG("[PhysX] Initialized successfully (Scene=%p, WorkerThreads=%u)", Scene, WorkerThreadCount);
	return true;
}

// 남은 시뮬레이션과 지연 명령을 정리한 뒤 PhysX scene 리소스를 해제한다.
void FPhysXPhysicsScene::ReleaseScene()
{
	ReleaseClothScene();
	WaitForSimulation();
	FlushDeferredSceneCommands();

	for (FPhysicsConstraintInstance* ConstraintInstance : StandaloneConstraintInstances)
	{
		if (!ConstraintInstance) continue;
		if (PxJoint* Joint = static_cast<PxJoint*>(ConstraintInstance->GetJointHandle().NativeJoint))
		{
			Joint->release();
		}
		UnregisterConstraintInstance(ConstraintInstance);
		delete ConstraintInstance;
	}
	StandaloneConstraintInstances.clear();
	GetMutableRuntimeStats().ConstraintCount = 0;

	{
		SCOPED_PHYSX_SCENE_WRITE_LOCK(Scene);
		for (FPhysicsBodyInstance* BodyInstance : StandaloneBodyInstances)
		{
			if (!BodyInstance) continue;
			if (PxRigidActor* Actor = static_cast<PxRigidActor*>(BodyInstance->GetActorHandle().NativeActor))
			{
				Scene->removeActor(*Actor);
				Actor->release();
			}
			UnregisterBodyInstance(BodyInstance);
			delete BodyInstance;
		}
	}
	StandaloneBodyInstances.clear();

	for (auto& Pair : BodyInstances) delete Pair.second;
	BodyInstances.clear();

	{
		SCOPED_PHYSX_SCENE_WRITE_LOCK(Scene);
		for (auto& Mapping : BodyMappings)
		{
			if (Mapping.Actor) { Mapping.Actor->release(); Mapping.Actor = nullptr; }
		}
	}
	BodyMappings.clear();

	if (DefaultMaterial) { DefaultMaterial->release(); DefaultMaterial = nullptr; }
	if (Scene)           { Scene->release();           Scene           = nullptr; }
	if (EventCallback)   { delete EventCallback;       EventCallback   = nullptr; }
	if (Dispatcher)      { Dispatcher->release();      Dispatcher      = nullptr; }

	Foundation = nullptr;
	Physics    = nullptr;
	ReleaseSharedPhysX();
	ClearRegisteredInstances();
	SetNativeSceneHandle(nullptr);
	World = nullptr;
}

// ============================================================
// Body 관리
// ============================================================

// component 기반 actor 매핑을 재사용하거나 생성해서 runtime body instance를 만든다.
FPhysicsBodyInstance* FPhysXPhysicsScene::CreateBody(UPrimitiveComponent* Comp, const FPhysicsBodyDesc& BodyDesc)
{
	if (!Comp || !Comp->CanCreatePhysicsBody()) return nullptr;
	WaitForSimulation();

	// 이미 등록된 경우 기존 인스턴스 반환
	auto ExistingIt = BodyInstances.find(Comp);
	if (ExistingIt != BodyInstances.end()) return ExistingIt->second;

	RegisterComponentInternal(Comp);

	FBodyMapping* Mapping = FindMappingByComponent(Comp);
	if (!Mapping || !Mapping->Actor)
	{
		return nullptr;
	}

	FPhysicsBodyInstance* Instance = new FPhysicsBodyInstance();
	Instance->SetOwnerComponent(Comp);
	Instance->SetBodyDesc(BodyDesc);

	// 매핑에서 PxRigidActor 핸들 연결
	FPhysicsActorHandle Handle;
	Handle.NativeActor = static_cast<void*>(Mapping->Actor);
	Instance->SetActorHandle(Handle);
	Instance->SetActorState(EPhysicsActorState::PAS_Added);

	BodyInstances[Comp] = Instance;
	RegisterBodyInstance(Instance);
	GetMutableRuntimeStats().BodyCount = static_cast<int32>(BodyInstances.size());
	return Instance;
}

FPhysicsBodyInstance* FPhysXPhysicsScene::CreateBodyAtTransform(
	UPrimitiveComponent* OwnerComponent,
	const FPhysicsBodyDesc& BodyDesc,
	const FTransform& WorldTransform,
	bool bSyncOwnerTransform)
{
	if (!Scene || !Physics || !DefaultMaterial || BodyDesc.Shapes.empty())
	{
		return nullptr;
	}

	const FPhysicsBodyDesc RuntimeBodyDesc = MakeEngineUnitBodyDesc(BodyDesc);

	WaitForSimulation();

	PxRigidActor* Actor = nullptr;
	PxRigidDynamic* DynamicActor = nullptr;
	const PxTransform BodyPose = ToPxTransform(WorldTransform);
	if (RuntimeBodyDesc.BodyType == EPhysicsBodyType::PBT_Static)
	{
		Actor = Physics->createRigidStatic(BodyPose);
	}
	else
	{
		DynamicActor = Physics->createRigidDynamic(BodyPose);
		if (DynamicActor)
		{
			DynamicActor->setActorFlag(
				PxActorFlag::eDISABLE_GRAVITY,
				OwnerComponent && !OwnerComponent->GetEnableGravity());
			DynamicActor->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
			DynamicActor->setLinearDamping((std::max)(0.0f, RuntimeBodyDesc.LinearDamping));
			DynamicActor->setAngularDamping((std::max)(0.0f, RuntimeBodyDesc.AngularDamping));
			DynamicActor->setRigidBodyFlag(
				PxRigidBodyFlag::eKINEMATIC,
				RuntimeBodyDesc.BodyType == EPhysicsBodyType::PBT_Kinematic);
			// IMPORTANT: mass/inertia must be computed after shapes are attached.
			// Computing inertia on a shape-less actor makes ragdoll joints unstable.
		}
		Actor = DynamicActor;
	}

	if (!Actor)
	{
		return nullptr;
	}

	int32 AttachedShapeCount = 0;
	for (const FPhysicsShapeDesc& ShapeDesc : RuntimeBodyDesc.Shapes)
	{
		PxShape* Shape = CreateShapeFromDesc(
			Physics,
			DefaultMaterial,
			ShapeDesc,
			OwnerComponent,
			RuntimeBodyDesc.bEnableSelfCollision);
		if (!Shape)
		{
			continue;
		}

		Actor->attachShape(*Shape);
		++AttachedShapeCount;
		Shape->release();
	}

	if (AttachedShapeCount == 0)
	{
		Actor->release();
		return nullptr;
	}

	if (DynamicActor)
	{
		const float Mass = (std::max)(0.001f, RuntimeBodyDesc.Mass);
		PxRigidBodyExt::setMassAndUpdateInertia(*DynamicActor, Mass);
		DynamicActor->setSolverIterationCounts(12, 4);

		const PxVec3 InvInertia = DynamicActor->getMassSpaceInvInertiaTensor();
		const FString OwnerName = OwnerComponent ? OwnerComponent->GetName() : FString("None");
	}

	Actor->userData = OwnerComponent ? OwnerComponent->GetOwner() : nullptr;
	{
		SCOPED_PHYSX_SCENE_WRITE_LOCK(Scene);
		Scene->addActor(*Actor);
	}

	if (bSyncOwnerTransform && OwnerComponent)
	{
		OwnerComponent->SetWorldLocation(WorldTransform.Location);
		OwnerComponent->SetRelativeRotation(WorldTransform.Rotation);
	}

	FPhysicsBodyInstance* Instance = new FPhysicsBodyInstance();
	Instance->SetOwnerComponent(OwnerComponent);
	Instance->SetBodyDesc(RuntimeBodyDesc);

	FPhysicsActorHandle Handle;
	Handle.NativeActor = static_cast<void*>(Actor);
	Instance->SetActorHandle(Handle);
	Instance->SetActorState(EPhysicsActorState::PAS_Added);

	StandaloneBodyInstances.push_back(Instance);
	RegisterBodyInstance(Instance);
	GetMutableRuntimeStats().BodyCount =
		static_cast<int32>(BodyInstances.size() + StandaloneBodyInstances.size());
	return Instance;
}

// runtime body instance와 연결된 PhysX actor를 scene에서 제거하고 해제한다.
void FPhysXPhysicsScene::DestroyBody(FPhysicsBodyInstance* BodyInstance)
{
	if (!BodyInstance) return;
	WaitForSimulation();

	auto StandaloneIt = std::find(
		StandaloneBodyInstances.begin(),
		StandaloneBodyInstances.end(),
		BodyInstance);
	if (StandaloneIt != StandaloneBodyInstances.end())
	{
		if (Scene)
		{
			SCOPED_PHYSX_SCENE_WRITE_LOCK(Scene);
			if (PxRigidActor* Actor = static_cast<PxRigidActor*>(BodyInstance->GetActorHandle().NativeActor))
			{
				Scene->removeActor(*Actor);
				Actor->release();
			}
		}

		StandaloneBodyInstances.erase(StandaloneIt);
		UnregisterBodyInstance(BodyInstance);
		BodyInstance->SetActorState(EPhysicsActorState::PAS_Destroyed);
		BodyInstance->GetMutableActorHandle().Reset();
		delete BodyInstance;

		GetMutableRuntimeStats().BodyCount =
			static_cast<int32>(BodyInstances.size() + StandaloneBodyInstances.size());
		return;
	}

	UPrimitiveComponent* Comp = BodyInstance->GetOwnerComponent();
	if (!Comp) return;

	UnregisterComponentInternal(Comp);
	BodyInstances.erase(Comp);
	UnregisterBodyInstance(BodyInstance);

	BodyInstance->SetActorState(EPhysicsActorState::PAS_Destroyed);
	BodyInstance->GetMutableActorHandle().Reset();
	delete BodyInstance;

	GetMutableRuntimeStats().BodyCount =
		static_cast<int32>(BodyInstances.size() + StandaloneBodyInstances.size());
}

bool FPhysXPhysicsScene::GetBodyWorldTransform(const FPhysicsBodyInstance* BodyInstance, FTransform& OutTransform) const
{
	if (!Scene || !BodyInstance || !BodyInstance->IsValidBodyInstance())
	{
		return false;
	}

	PxRigidActor* Actor = static_cast<PxRigidActor*>(BodyInstance->GetActorHandle().NativeActor);
	if (!Actor)
	{
		return false;
	}

	SCOPED_PHYSX_SCENE_READ_LOCK(Scene);
	OutTransform = ToFTransform(Actor->getGlobalPose());
	return true;
}

void FPhysXPhysicsScene::SetBodyWorldTransform(FPhysicsBodyInstance* BodyInstance, const FTransform& WorldTransform)
{
	if (!Scene || !BodyInstance || !BodyInstance->IsValidBodyInstance())
	{
		return;
	}

	PxRigidActor* Actor = static_cast<PxRigidActor*>(BodyInstance->GetActorHandle().NativeActor);
	if (!Actor)
	{
		return;
	}

	WaitForSimulation();
	SCOPED_PHYSX_SCENE_WRITE_LOCK(Scene);
	const PxTransform NewPose = ToPxTransform(WorldTransform);
	if (PxRigidDynamic* DynamicActor = Actor->is<PxRigidDynamic>())
	{
		if (DynamicActor->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC)
		{
			DynamicActor->setKinematicTarget(NewPose);
		}
		else
		{
			DynamicActor->setGlobalPose(NewPose);
		}
	}
	else
	{
		Actor->setGlobalPose(NewPose);
	}
}

FPhysicsConstraintInstance* FPhysXPhysicsScene::CreateConstraint(
	FPhysicsBodyInstance* ParentBody,
	FPhysicsBodyInstance* ChildBody,
	const FPhysicsConstraintDesc& ConstraintDesc)
{
	if (!Scene || !Physics || !ParentBody || !ChildBody ||
		!ParentBody->IsValidBodyInstance() || !ChildBody->IsValidBodyInstance())
	{
		return nullptr;
	}

	PxRigidActor* ParentActor = static_cast<PxRigidActor*>(ParentBody->GetActorHandle().NativeActor);
	PxRigidActor* ChildActor  = static_cast<PxRigidActor*>(ChildBody->GetActorHandle().NativeActor);
	if (!ParentActor || !ChildActor)
	{
		return nullptr;
	}

	WaitForSimulation();

	const FPhysicsConstraintDesc RuntimeConstraintDesc = MakeEngineUnitConstraintDesc(ConstraintDesc);

	PxJoint* Joint = nullptr;
	const PxTransform ParentLocalFrame = ToPxTransform(RuntimeConstraintDesc.ParentLocalFrame);
	const PxTransform ChildLocalFrame  = ToPxTransform(RuntimeConstraintDesc.ChildLocalFrame);

	if (RuntimeConstraintDesc.JointType == EPhysicsJointType::PJT_Fixed)
	{
		Joint = PxFixedJointCreate(*Physics, ParentActor, ParentLocalFrame, ChildActor, ChildLocalFrame);
	}
	else
	{
		PxD6Joint* D6Joint = PxD6JointCreate(*Physics, ParentActor, ParentLocalFrame, ChildActor, ChildLocalFrame);
		if (D6Joint)
		{
			D6Joint->setMotion(PxD6Axis::eX,      ToPxD6Motion(RuntimeConstraintDesc.XMotion));
			D6Joint->setMotion(PxD6Axis::eY,      ToPxD6Motion(RuntimeConstraintDesc.YMotion));
			D6Joint->setMotion(PxD6Axis::eZ,      ToPxD6Motion(RuntimeConstraintDesc.ZMotion));
			D6Joint->setMotion(PxD6Axis::eSWING1, ToPxD6Motion(RuntimeConstraintDesc.Swing1Motion));
			D6Joint->setMotion(PxD6Axis::eSWING2, ToPxD6Motion(RuntimeConstraintDesc.Swing2Motion));
			D6Joint->setMotion(PxD6Axis::eTWIST,  ToPxD6Motion(RuntimeConstraintDesc.TwistMotion));

			if (RuntimeConstraintDesc.XMotion == EPhysicsConstraintMotionMode::Limited ||
				RuntimeConstraintDesc.YMotion == EPhysicsConstraintMotionMode::Limited ||
				RuntimeConstraintDesc.ZMotion == EPhysicsConstraintMotionMode::Limited)
			{
				PxJointLinearLimit LinearLimit(Physics->getTolerancesScale(), (std::max)(0.01f, RuntimeConstraintDesc.LinearLimit));
				LinearLimit.stiffness = RuntimeConstraintDesc.Stiffness;
				LinearLimit.damping   = RuntimeConstraintDesc.Damping;
				D6Joint->setLinearLimit(LinearLimit);
			}

			if (RuntimeConstraintDesc.Swing1Motion == EPhysicsConstraintMotionMode::Limited ||
				RuntimeConstraintDesc.Swing2Motion == EPhysicsConstraintMotionMode::Limited)
			{
				PxJointLimitCone SwingLimit(
					(std::max)(0.001f, ToRadians(RuntimeConstraintDesc.SwingLimitY)),
					(std::max)(0.001f, ToRadians(RuntimeConstraintDesc.SwingLimitZ)));
				SwingLimit.stiffness = RuntimeConstraintDesc.Stiffness;
				SwingLimit.damping   = RuntimeConstraintDesc.Damping;
				D6Joint->setSwingLimit(SwingLimit);
			}

			if (RuntimeConstraintDesc.TwistMotion == EPhysicsConstraintMotionMode::Limited)
			{
				float TwistMin = ToRadians(RuntimeConstraintDesc.TwistLimitMin);
				float TwistMax = ToRadians(RuntimeConstraintDesc.TwistLimitMax);
				if (TwistMin > TwistMax)
				{
					std::swap(TwistMin, TwistMax);
				}
				PxJointAngularLimitPair TwistLimit(TwistMin, TwistMax);
				TwistLimit.stiffness = RuntimeConstraintDesc.Stiffness;
				TwistLimit.damping   = RuntimeConstraintDesc.Damping;
				D6Joint->setTwistLimit(TwistLimit);
			}
		}
		Joint = D6Joint;
	}

	if (!Joint)
	{
		return nullptr;
	}

	Joint->setConstraintFlag(PxConstraintFlag::eCOLLISION_ENABLED, !RuntimeConstraintDesc.bDisableCollision);

	FPhysicsConstraintInstance* Instance = new FPhysicsConstraintInstance();
	Instance->SetParentBody(ParentBody);
	Instance->SetChildBody(ChildBody);
	Instance->SetConstraintDesc(RuntimeConstraintDesc);

	FPhysicsJointHandle Handle;
	Handle.NativeJoint = static_cast<void*>(Joint);
	Instance->SetJointHandle(Handle);

	StandaloneConstraintInstances.push_back(Instance);
	RegisterConstraintInstance(Instance);
	GetMutableRuntimeStats().ConstraintCount = static_cast<int32>(StandaloneConstraintInstances.size());
	return Instance;
}

void FPhysXPhysicsScene::DestroyConstraint(FPhysicsConstraintInstance* ConstraintInstance)
{
	if (!ConstraintInstance)
	{
		return;
	}

	WaitForSimulation();
	auto It = std::find(
		StandaloneConstraintInstances.begin(),
		StandaloneConstraintInstances.end(),
		ConstraintInstance);

	if (PxJoint* Joint = static_cast<PxJoint*>(ConstraintInstance->GetJointHandle().NativeJoint))
	{
		Joint->release();
	}

	if (It != StandaloneConstraintInstances.end())
	{
		StandaloneConstraintInstances.erase(It);
	}
	UnregisterConstraintInstance(ConstraintInstance);
	ConstraintInstance->GetMutableJointHandle().Reset();
	delete ConstraintInstance;

	GetMutableRuntimeStats().ConstraintCount = static_cast<int32>(StandaloneConstraintInstances.size());
}

// ============================================================
// Internal Register/Unregister (기존 RegisterComponent 로직)
// ============================================================

void FPhysXPhysicsScene::RegisterComponentInternal(UPrimitiveComponent* Comp)
{
	if (!Comp || !Scene || !Physics || !DefaultMaterial) return;
	if (!Comp->CanCreatePhysicsBody()) return;
	if (FindMappingByComponent(Comp)) return;
	WaitForSimulation();

	AActor* OwnerActor = Comp->GetOwner();
	if (!OwnerActor) return;

	const bool bStandaloneShapeActor = Comp && Comp->IsA<UShapeComponent>();
	FBodyMapping* Mapping = bStandaloneShapeActor ? nullptr : FindMappingByActor(OwnerActor);
	if (!Mapping)
	{
		UPrimitiveComponent* RootPrim = bStandaloneShapeActor
			? Comp
			: Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent());
		if (!RootPrim) RootPrim = Comp;

		const bool bDynamic = RootPrim->GetSimulatePhysics();
		PxTransform BodyXf  = GetPxTransform(RootPrim);

		PxRigidActor* Body = bDynamic
			? static_cast<PxRigidActor*>(Physics->createRigidDynamic(BodyXf))
			: static_cast<PxRigidActor*>(Physics->createRigidStatic(BodyXf));
		if (!Body) return;

		if (PxRigidDynamic* DynamicBody = Body->is<PxRigidDynamic>())
		{
			DynamicBody->setActorFlag(PxActorFlag::eDISABLE_GRAVITY, !RootPrim->GetEnableGravity());
			DynamicBody->setRigidBodyFlag(PxRigidBodyFlag::eENABLE_CCD, true);
		}

		Body->userData = OwnerActor;
		{
			SCOPED_PHYSX_SCENE_WRITE_LOCK(Scene);
			Scene->addActor(*Body);
		}

		FBodyMapping NewMapping;
		NewMapping.OwnerActor = OwnerActor;
		NewMapping.Actor      = Body;
		NewMapping.RootComp   = RootPrim;
		NewMapping.bStandaloneShapeActor = bStandaloneShapeActor;
		BodyMappings.push_back(NewMapping);
		Mapping = &BodyMappings.back();
	}

	{
		SCOPED_PHYSX_SCENE_WRITE_LOCK(Scene);
		PxShape* Shape = AddShapeForComponent(*Mapping, Comp);
		if (!Shape)
		{
			if (Mapping->Components.empty())
			{
				if (Mapping->Actor)
				{
					Scene->removeActor(*Mapping->Actor);
					Mapping->Actor->release();
				}
				*Mapping = BodyMappings.back();
				BodyMappings.pop_back();
			}
			return;
		}
		Mapping->Components.push_back(Comp);

		if (PxRigidDynamic* Dyn = Mapping->Actor->is<PxRigidDynamic>())
			ApplyRootMassAndCOM(Dyn, Mapping->RootComp);
	}
}

void FPhysXPhysicsScene::UnregisterComponentInternal(UPrimitiveComponent* Comp)
{
	if (!Comp || !Scene) return;
	WaitForSimulation();

	FBodyMapping* Mapping = FindMappingByComponent(Comp);
	if (!Mapping) return;

	{
		SCOPED_PHYSX_SCENE_WRITE_LOCK(Scene);
		DetachShapeForComponent(*Mapping, Comp);
		Mapping->Components.erase(
			std::remove(Mapping->Components.begin(), Mapping->Components.end(), Comp),
			Mapping->Components.end());

		if (Mapping->Components.empty())
		{
			if (Mapping->Actor) { Scene->removeActor(*Mapping->Actor); Mapping->Actor->release(); }
			*Mapping = BodyMappings.back();
			BodyMappings.pop_back();
			return;
		}
		if (PxRigidDynamic* Dyn = Mapping->Actor->is<PxRigidDynamic>())
			ApplyRootMassAndCOM(Dyn, Mapping->RootComp);
	}
}

void FPhysXPhysicsScene::RebuildBody(UPrimitiveComponent* Comp)
{
	if (!Comp || !Scene) return;
	WaitForSimulation();

	FBodyMapping* Mapping = FindMappingByComponent(Comp);
	if (!Mapping) return;

	TArray<UPrimitiveComponent*> CompList = Mapping->Components;
	for (UPrimitiveComponent* C : CompList) UnregisterComponentInternal(C);
	for (UPrimitiveComponent* C : CompList) RegisterComponentInternal(C);

	// BodyInstance 핸들 갱신
	for (UPrimitiveComponent* C : CompList)
	{
		auto It = BodyInstances.find(C);
		if (It == BodyInstances.end()) continue;
		FBodyMapping* NewMapping = FindMappingByComponent(C);
		if (NewMapping && NewMapping->Actor)
		{
			FPhysicsActorHandle Handle;
			Handle.NativeActor = static_cast<void*>(NewMapping->Actor);
			It->second->SetActorHandle(Handle);
		}
	}
}

// 진행 중인 PhysX simulation이 있으면 결과를 받아 scene을 안전한 쓰기 상태로 되돌린다.
void FPhysXPhysicsScene::WaitForSimulation()
{
	if (!Scene)
	{
		bSimulationInFlight.store(false);
		return;
	}

	if (bSimulationInFlight.load())
	{
		FetchResults(true);
		return;
	}

	FlushDeferredSceneCommands();
}

// simulation 중에는 scene write 명령을 큐에 넣고, 안전한 시점에는 즉시 write lock 안에서 실행한다.
void FPhysXPhysicsScene::ExecuteOrDeferSceneWrite(std::function<void()> Command)
{
	if (!Command || !Scene)
	{
		return;
	}

	if (bSimulationInFlight.load())
	{
		std::lock_guard<std::mutex> Lock(DeferredSceneCommandMutex);
		DeferredSceneCommands.push_back(std::move(Command));
		return;
	}

	SCOPED_PHYSX_SCENE_WRITE_LOCK(Scene);
	Command();
}

// FetchResults 이후 또는 다음 simulation 직전에 누적된 scene write 명령을 일괄 실행한다.
void FPhysXPhysicsScene::FlushDeferredSceneCommands()
{
	if (!Scene)
	{
		return;
	}

	std::vector<std::function<void()>> LocalCommands;
	{
		std::lock_guard<std::mutex> Lock(DeferredSceneCommandMutex);
		if (DeferredSceneCommands.empty())
		{
			return;
		}
		LocalCommands.swap(DeferredSceneCommands);
	}

	SCOPED_PHYSX_SCENE_WRITE_LOCK(Scene);
	for (std::function<void()>& Command : LocalCommands)
	{
		if (Command)
		{
			Command();
		}
	}
}

// simulation 전에 엔진 Transform을 PhysX actor pose로 동기화한다.
void FPhysXPhysicsScene::SyncEngineTransformsToPhysX()
{
	if (!Scene)
	{
		return;
	}

	SCOPED_PHYSX_SCENE_WRITE_LOCK(Scene);

	constexpr float TeleportPosThresholdSq = 1.0f;
	constexpr float TeleportRotThreshold   = 0.99f;

	for (auto& Mapping : BodyMappings)
	{
		if (!Mapping.RootComp || !Mapping.Actor) continue;
		PxTransform NewPose = GetPxTransform(Mapping.RootComp);

		if (PxRigidDynamic* Dynamic = Mapping.Actor->is<PxRigidDynamic>())
		{
			if (Dynamic->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC)
			{
				Dynamic->setKinematicTarget(NewPose);
			}
			else
			{
				PxTransform PxPose = Dynamic->getGlobalPose();
				PxVec3 dp = NewPose.p - PxPose.p;
				const float DistSq = dp.x*dp.x + dp.y*dp.y + dp.z*dp.z;
				const float QDot = std::abs(
					NewPose.q.x*PxPose.q.x + NewPose.q.y*PxPose.q.y +
					NewPose.q.z*PxPose.q.z + NewPose.q.w*PxPose.q.w);
				if (DistSq > TeleportPosThresholdSq || QDot < TeleportRotThreshold)
					Dynamic->setGlobalPose(NewPose);
			}
		}
		else if (Mapping.Actor->is<PxRigidStatic>())
		{
			Mapping.Actor->setGlobalPose(NewPose);
		}
	}
}

// FetchResults 이후 PhysX actor pose를 엔진 component Transform으로 동기화한다.
void FPhysXPhysicsScene::SyncPhysXTransformsToEngine()
{
	if (!Scene)
	{
		return;
	}

	struct FPendingTransformSync
	{
		UPrimitiveComponent* RootComp = nullptr;
		FVector Location;
		FQuat Rotation;
	};

	std::vector<FPendingTransformSync> PendingTransforms;
	{
		SCOPED_PHYSX_SCENE_READ_LOCK(Scene);

		for (auto& Mapping : BodyMappings)
		{
			if (!Mapping.RootComp || !Mapping.Actor) continue;
			PxRigidDynamic* Dynamic = Mapping.Actor->is<PxRigidDynamic>();
			if (!Dynamic) continue;
			if (Dynamic->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC) continue;
			if (Dynamic->isSleeping()) continue;

			PxTransform Pose = Dynamic->getGlobalPose();
			PendingTransforms.push_back({ Mapping.RootComp, ToFVector(Pose.p), ToFQuat(Pose.q) });
		}
	}

	for (const FPendingTransformSync& PendingTransform : PendingTransforms)
	{
		if (!PendingTransform.RootComp) continue;
		PendingTransform.RootComp->SetWorldLocation(PendingTransform.Location);
		PendingTransform.RootComp->SetRelativeRotation(PendingTransform.Rotation);
	}
}

// ============================================================
// 시뮬레이션 (구 Tick 분리)
// ============================================================

// PhysX simulation step을 시작하고, scene write 명령은 simulation 전후 안전한 시점으로 보낸다.
void FPhysXPhysicsScene::SimulateRigid(const FPhysicsStepInfo& StepInfo)
{
	if (!Scene) return;
	float DeltaTime = StepInfo.DeltaTime;
	if (DeltaTime <= 0.0f) return;

	constexpr float MaxPhysicsDeltaTime = 0.1f;
	if (DeltaTime > MaxPhysicsDeltaTime) DeltaTime = MaxPhysicsDeltaTime;

	WaitForSimulation();
	FlushDeferredSceneCommands();
	SyncEngineTransformsToPhysX();

	UpdateVehicles(DeltaTime);
	Scene->simulate(DeltaTime);
	bSimulationInFlight.store(true);
}


void FPhysXPhysicsScene::Simulate(const FPhysicsStepInfo& StepInfo)
{
	SimulateRigid(StepInfo);
}

// PhysX simulation 결과를 기다리고, actor pose와 지연 scene write 명령을 처리한다.
void FPhysXPhysicsScene::FetchResults(bool bBlock)
{
	if (!Scene) return;
	if (bSimulationInFlight.load())
	{
		if (!Scene->fetchResults(bBlock))
		{
			return;
		}
		bSimulationInFlight.store(false);
	}

	{
		SCOPED_PHYSX_SCENE_READ_LOCK(Scene);
		for (const FPhysicsBodyInstance* BodyInstance : StandaloneBodyInstances)
		{
			if (!BodyInstance || !BodyInstance->IsValidBodyInstance())
			{
				continue;
			}

			PxRigidActor* Actor = static_cast<PxRigidActor*>(BodyInstance->GetActorHandle().NativeActor);
			PxRigidDynamic* DynamicActor = Actor ? Actor->is<PxRigidDynamic>() : nullptr;
			if (!DynamicActor)
			{
				continue;
			}

			const PxVec3 LinearVelocity = DynamicActor->getLinearVelocity();
			const PxVec3 AngularVelocity = DynamicActor->getAngularVelocity();
			const float LinearSpeed = PxVecLength(LinearVelocity);
			const float AngularSpeed = PxVecLength(AngularVelocity);
		}
	}

	SyncPhysXTransformsToEngine();
	// Vehicle Pose 동기화
	SyncVehiclePose();
	// Dispatch deferred contact/trigger events
	if (EventCallback) EventCallback->DispatchPendingEvents();
	FlushDeferredSceneCommands();

	GetMutableRuntimeStats().ContactCount = 0; // PhysX 측 contact 수는 콜백에서 집계 가능 (현재 생략)
}


void FPhysXPhysicsScene::GatherClothCollision(
	const FClothCollisionGatherParams& Params,
	FClothCollisionData& Out) const
{
	Out.Reset();

	if (!Scene)
	{
		return;
	}

	// This must be called after FetchResults(). If a caller violates the fixed-step order,
	// do not read half-simulated poses and return no collision instead.
	if (bSimulationInFlight.load())
	{
		return;
	}

	std::unordered_set<const PxRigidActor*> VisitedActors;
	SCOPED_PHYSX_SCENE_READ_LOCK(Scene);

	// RuntimeBodyInstances cover normal primitive bodies and any body registered through the base scene.
	for (const FPhysicsBodyInstance* BodyInstance : GetBodyInstances())
	{
		if (!BodyInstance || !BodyInstance->IsValidBodyInstance())
		{
			continue;
		}

		PxRigidActor* Actor = static_cast<PxRigidActor*>(BodyInstance->GetActorHandle().NativeActor);
		if (!Actor || !VisitedActors.insert(Actor).second)
		{
			continue;
		}
		GatherClothCollisionFromActor(Actor, Params, Out);
	}

	// StandaloneBodyInstances are iterated explicitly as well for ragdoll / desc-created bodies.
	for (const FPhysicsBodyInstance* BodyInstance : StandaloneBodyInstances)
	{
		if (!BodyInstance || !BodyInstance->IsValidBodyInstance())
		{
			continue;
		}

		PxRigidActor* Actor = static_cast<PxRigidActor*>(BodyInstance->GetActorHandle().NativeActor);
		if (!Actor || !VisitedActors.insert(Actor).second)
		{
			continue;
		}
		GatherClothCollisionFromActor(Actor, Params, Out);
	}
}

// ============================================================
// AddShapeForComponent / DetachShapeForComponent
// ============================================================

PxShape* FPhysXPhysicsScene::AddShapeForComponent(FBodyMapping& Mapping, UPrimitiveComponent* Comp)
{
	if (!Mapping.Actor || !DefaultMaterial || !Comp) return nullptr;

	PxGeometryHolder Geom;
	bool bHasGeom = false;
	PxQuat ShapeAxisRot = PxQuat(PxIdentity);
	FVector ShapeLocalOffset = FVector::ZeroVector;

	if (auto* Box = Cast<UBoxComponent>(Comp))
	{
		FVector Ext = Box->GetScaledBoxExtent();
		Geom = PxBoxGeometry(Ext.X, Ext.Y, Ext.Z);
		bHasGeom = true;
	}
	else if (auto* Sphere = Cast<USphereComponent>(Comp))
	{
		Geom = PxSphereGeometry(Sphere->GetScaledSphereRadius());
		bHasGeom = true;
	}
	else if (auto* Capsule = Cast<UCapsuleComponent>(Comp))
	{
		float Radius = Capsule->GetScaledCapsuleRadius();
		float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		Geom = PxCapsuleGeometry(Radius, HalfHeight - Radius);
		// PhysX capsules are aligned to the local X axis. UCapsuleComponent is
		// authored/drawn along local Z, so rotate the PhysX shape X axis onto Z.
		// The previous Z-axis rotation mapped X to Y and made visual capsule/cloth
		// collision disagree.
		ShapeAxisRot = PxQuat(PxHalfPi, PxVec3(0.0f, -1.0f, 0.0f));
		bHasGeom = true;
	}
	else if (auto* StaticMesh = Cast<UStaticMeshComponent>(Comp))
	{
		FVector LocalCenter;
		FVector LocalExtent;
		if (StaticMesh->GetLocalBounds(LocalCenter, LocalExtent))
		{
			const FVector Scale = StaticMesh->GetWorldScale();
			Geom = PxBoxGeometry(
				(std::max)(0.01f, std::abs(LocalExtent.X * Scale.X)),
				(std::max)(0.01f, std::abs(LocalExtent.Y * Scale.Y)),
				(std::max)(0.01f, std::abs(LocalExtent.Z * Scale.Z)));
			ShapeLocalOffset = FVector(
				LocalCenter.X * Scale.X,
				LocalCenter.Y * Scale.Y,
				LocalCenter.Z * Scale.Z);
			bHasGeom = true;
		}
	}
	if (!bHasGeom) return nullptr;

	PxShape* Shape = PxRigidActorExt::createExclusiveShape(*Mapping.Actor, Geom.any(), *DefaultMaterial);
	if (!Shape) return nullptr;

	PxTransform LocalPose = PxTransform(PxIdentity);
	if (Comp != Mapping.RootComp && Mapping.RootComp)
	{
		FVector RootPos = Mapping.RootComp->GetWorldLocation();
		FQuat   RootRot = Mapping.RootComp->GetWorldMatrix().ToQuat();
		FVector CompPos = Comp->GetWorldLocation();
		FQuat   CompRot = Comp->GetWorldMatrix().ToQuat();

		FQuat InvRootRot = RootRot.Inverse();
		FVector LocalPos = InvRootRot.RotateVector(CompPos - RootPos);
		FQuat   LocalRot = InvRootRot * CompRot;
		LocalPose = PxTransform(ToPxVec3(LocalPos), ToPxQuat(LocalRot));
	}
	if (!ShapeLocalOffset.IsNearlyZero())
	{
		LocalPose.p += LocalPose.q.rotate(ToPxVec3(ShapeLocalOffset));
	}
	LocalPose.q = LocalPose.q * ShapeAxisRot;
	Shape->setLocalPose(LocalPose);

	SetupFilterData(Shape, Comp);
	Shape->setFlag(PxShapeFlag::eSCENE_QUERY_SHAPE, Comp->IsQueryCollisionEnabled());
	Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, Comp->IsPhysicsCollisionEnabled());
	SetupVehicleQueryFilterData(Shape, Comp, IsVehicleDrivableSurface(Comp));

	bool bShouldBeTrigger = Comp->GetGenerateOverlapEvents();
	if (!bShouldBeTrigger)
	{
		bool bHasAnyBlockResponse = false;
		for (int32 Ch = 0; Ch < NumActiveCollisionChannels; ++Ch)
		{
			if (Comp->GetCollisionResponseToChannel(static_cast<ECollisionChannel>(Ch)) == ECollisionResponse::Block)
			{ bHasAnyBlockResponse = true; break; }
		}
		bShouldBeTrigger = !bHasAnyBlockResponse;
	}
	if (bShouldBeTrigger)
	{
		Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
		Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
	}

	Shape->userData = Comp;
	return Shape;
}

void FPhysXPhysicsScene::DetachShapeForComponent(FBodyMapping& Mapping, UPrimitiveComponent* Comp)
{
	if (!Mapping.Actor || !Comp) return;
	const PxU32 NumShapes = Mapping.Actor->getNbShapes();
	if (NumShapes == 0) return;

	std::vector<PxShape*> Shapes(NumShapes);
	Mapping.Actor->getShapes(Shapes.data(), NumShapes);
	for (PxShape* Shape : Shapes)
	{
		if (Shape && Shape->userData == Comp)
		{ Mapping.Actor->detachShape(*Shape); break; }
	}
}

void FPhysXPhysicsScene::SetComponentVehicleDrivableSurface(UPrimitiveComponent* Comp, bool bDrivable)
{
	if (!Comp) return;

	FBodyMapping* Mapping = FindMappingByComponent(Comp);
	if (!Mapping || !Mapping->Actor) return;

	const PxU32 NumShapes = Mapping->Actor->getNbShapes();
	if (NumShapes == 0) return;

	std::vector<PxShape*> Shapes(NumShapes);
	Mapping->Actor->getShapes(Shapes.data(), NumShapes);
	for (PxShape* Shape : Shapes)
	{
		if (!Shape || Shape->userData != Comp) continue;

		SetupVehicleQueryFilterData(Shape, Comp, bDrivable);
	}
}

// ============================================================
// Mapping helpers
// ============================================================

FPhysXPhysicsScene::FBodyMapping* FPhysXPhysicsScene::FindMappingByActor(AActor* OwnerActor)
{
	for (auto& M : BodyMappings)
	{
		if (M.OwnerActor == OwnerActor && !M.bStandaloneShapeActor)
			return &M;
	}
	return nullptr;
}
const FPhysXPhysicsScene::FBodyMapping* FPhysXPhysicsScene::FindMappingByActor(AActor* OwnerActor) const
{
	for (const auto& M : BodyMappings)
	{
		if (M.OwnerActor == OwnerActor && !M.bStandaloneShapeActor)
			return &M;
	}
	return nullptr;
}
FPhysXPhysicsScene::FBodyMapping* FPhysXPhysicsScene::FindMappingByComponent(UPrimitiveComponent* Comp)
{
	if (!Comp) return nullptr;
	for (auto& M : BodyMappings) for (UPrimitiveComponent* C : M.Components) if (C == Comp) return &M;
	return nullptr;
}
const FPhysXPhysicsScene::FBodyMapping* FPhysXPhysicsScene::FindMappingByComponent(UPrimitiveComponent* Comp) const
{
	if (!Comp) return nullptr;
	for (const auto& M : BodyMappings) for (UPrimitiveComponent* C : M.Components) if (C == Comp) return &M;
	return nullptr;
}

// ============================================================
// Force / Torque
// ============================================================

void FPhysXPhysicsScene::AddForce(UPrimitiveComponent* Comp, const FVector& Force)
{
	FBodyMapping* M = FindMappingByComponent(Comp);
	if (!M || !M->Actor) return;
	PxRigidActor* Actor = M->Actor;
	ExecuteOrDeferSceneWrite([Actor, Force]()
	{
		PxRigidDynamic* Dyn = Actor ? Actor->is<PxRigidDynamic>() : nullptr;
		if (Dyn) Dyn->addForce(ToPxVec3(Force));
	});
}
void FPhysXPhysicsScene::AddForceAtLocation(UPrimitiveComponent* Comp, const FVector& Force, const FVector& WorldLocation)
{
	FBodyMapping* M = FindMappingByComponent(Comp);
	if (!M || !M->Actor) return;
	PxRigidActor* Actor = M->Actor;
	ExecuteOrDeferSceneWrite([Actor, Force, WorldLocation]()
	{
		PxRigidDynamic* Dyn = Actor ? Actor->is<PxRigidDynamic>() : nullptr;
		if (Dyn) PxRigidBodyExt::addForceAtPos(*Dyn, ToPxVec3(Force), ToPxVec3(WorldLocation));
	});
}
void FPhysXPhysicsScene::AddTorque(UPrimitiveComponent* Comp, const FVector& Torque)
{
	FBodyMapping* M = FindMappingByComponent(Comp);
	if (!M || !M->Actor) return;
	PxRigidActor* Actor = M->Actor;
	ExecuteOrDeferSceneWrite([Actor, Torque]()
	{
		PxRigidDynamic* Dyn = Actor ? Actor->is<PxRigidDynamic>() : nullptr;
		if (Dyn) Dyn->addTorque(ToPxVec3(Torque));
	});
}

// ============================================================
// Velocity
// ============================================================

FVector FPhysXPhysicsScene::GetLinearVelocity(UPrimitiveComponent* Comp) const
{
	const FBodyMapping* M = FindMappingByComponent(Comp);
	if (!M || !M->Actor) return { 0, 0, 0 };
	SCOPED_PHYSX_SCENE_READ_LOCK(Scene);
	PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
	return Dyn ? ToFVector(Dyn->getLinearVelocity()) : FVector(0, 0, 0);
}
void FPhysXPhysicsScene::SetLinearVelocity(UPrimitiveComponent* Comp, const FVector& Vel)
{
	FBodyMapping* M = FindMappingByComponent(Comp);
	if (!M || !M->Actor) return;
	PxRigidActor* Actor = M->Actor;
	ExecuteOrDeferSceneWrite([Actor, Vel]()
	{
		PxRigidDynamic* Dyn = Actor ? Actor->is<PxRigidDynamic>() : nullptr;
		if (Dyn) Dyn->setLinearVelocity(ToPxVec3(Vel));
	});
}
FVector FPhysXPhysicsScene::GetAngularVelocity(UPrimitiveComponent* Comp) const
{
	const FBodyMapping* M = FindMappingByComponent(Comp);
	if (!M || !M->Actor) return { 0, 0, 0 };
	SCOPED_PHYSX_SCENE_READ_LOCK(Scene);
	PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
	return Dyn ? ToFVector(Dyn->getAngularVelocity()) : FVector(0, 0, 0);
}
void FPhysXPhysicsScene::SetAngularVelocity(UPrimitiveComponent* Comp, const FVector& Vel)
{
	FBodyMapping* M = FindMappingByComponent(Comp);
	if (!M || !M->Actor) return;
	PxRigidActor* Actor = M->Actor;
	ExecuteOrDeferSceneWrite([Actor, Vel]()
	{
		PxRigidDynamic* Dyn = Actor ? Actor->is<PxRigidDynamic>() : nullptr;
		if (Dyn) Dyn->setAngularVelocity(ToPxVec3(Vel));
	});
}

// ============================================================
// Mass
// ============================================================

void FPhysXPhysicsScene::SetMass(UPrimitiveComponent* Comp, float NewMass)
{
	FBodyMapping* M = FindMappingByComponent(Comp);
	if (!M || !M->Actor) return;
	PxRigidActor* Actor = M->Actor;
	PxVec3 LocalCOM = M->RootComp ? ToPxVec3(M->RootComp->GetCenterOfMass()) : PxVec3(0);
	ExecuteOrDeferSceneWrite([Actor, NewMass, LocalCOM]()
	{
		PxRigidDynamic* Dyn = Actor ? Actor->is<PxRigidDynamic>() : nullptr;
		if (Dyn) PxRigidBodyExt::setMassAndUpdateInertia(*Dyn, NewMass, &LocalCOM);
	});
}
float FPhysXPhysicsScene::GetMass(UPrimitiveComponent* Comp) const
{
	const FBodyMapping* M = FindMappingByComponent(Comp);
	if (!M || !M->Actor) return 1.0f;
	SCOPED_PHYSX_SCENE_READ_LOCK(Scene);
	PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
	return Dyn ? Dyn->getMass() : 1.0f;
}
void FPhysXPhysicsScene::SetCenterOfMass(UPrimitiveComponent* Comp, const FVector& LocalOffset)
{
	FBodyMapping* M = FindMappingByComponent(Comp);
	if (!M || !M->Actor) return;
	PxRigidActor* Actor = M->Actor;
	ExecuteOrDeferSceneWrite([Actor, LocalOffset]()
	{
		PxRigidDynamic* Dyn = Actor ? Actor->is<PxRigidDynamic>() : nullptr;
		if (Dyn) Dyn->setCMassLocalPose(PxTransform(ToPxVec3(LocalOffset)));
	});
}
FVector FPhysXPhysicsScene::GetCenterOfMass(UPrimitiveComponent* Comp) const
{
	const FBodyMapping* M = FindMappingByComponent(Comp);
	if (!M || !M->Actor) return { 0, 0, 0 };
	SCOPED_PHYSX_SCENE_READ_LOCK(Scene);
	PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
	return Dyn ? ToFVector(Dyn->getCMassLocalPose().p) : FVector(0, 0, 0);
}

// ============================================================
// Raycast (Start + End 시그니처)
// ============================================================

bool FPhysXPhysicsScene::Raycast(const FVector& Start, const FVector& End, FHitResult& OutHit,
	ECollisionChannel TraceChannel, const AActor* IgnoreActor) const
{
	if (!Scene) return false;

	FVector Move = End - Start;
	float MaxDist2 = Move.X*Move.X + Move.Y*Move.Y + Move.Z*Move.Z;
	if (MaxDist2 < 1e-12f) return false;
	const float MaxDist = sqrtf(MaxDist2);
	const FVector Dir(Move.X/MaxDist, Move.Y/MaxDist, Move.Z/MaxDist);

	struct FChannelRaycastFilter : PxQueryFilterCallback
	{
		const AActor* IgnoreActor = nullptr;
		PxU32 TraceBit = 0;
		FChannelRaycastFilter(const AActor* A, ECollisionChannel Ch)
			: IgnoreActor(A), TraceBit(1u << static_cast<PxU32>(Ch)) {}
		PxQueryHitType::Enum preFilter(const PxFilterData&, const PxShape* Shape,
			const PxRigidActor* Actor, PxHitFlags&) override
		{
			if (IgnoreActor && Actor && Actor->userData == IgnoreActor) return PxQueryHitType::eNONE;
			if (Shape && (Shape->getQueryFilterData().word1 & TraceBit) == 0) return PxQueryHitType::eNONE;
			return PxQueryHitType::eBLOCK;
		}
		PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&) override
		{ return PxQueryHitType::eBLOCK; }
	};

	PxRaycastBuffer Hit;
	PxQueryFilterData FilterData;
	FilterData.flags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER;
	FChannelRaycastFilter FilterCallback(IgnoreActor, TraceChannel);

	SCOPED_PHYSX_SCENE_READ_LOCK(Scene);
	bool bStatus = Scene->raycast(ToPxVec3(Start), ToPxVec3(Dir), MaxDist,
		Hit, PxHitFlag::eDEFAULT, FilterData, &FilterCallback);
	if (!bStatus || !Hit.hasBlock) return false;

	const PxRaycastHit& Block = Hit.block;
	OutHit.bHit            = true;
	OutHit.Distance        = Block.distance;
	OutHit.WorldHitLocation= ToFVector(Block.position);
	OutHit.ShapeLocation   = OutHit.WorldHitLocation;
	OutHit.ImpactNormal    = ToFVector(Block.normal);
	OutHit.WorldNormal     = OutHit.ImpactNormal;

	if (Block.shape && Block.shape->userData)
	{
		OutHit.HitComponent = static_cast<UPrimitiveComponent*>(Block.shape->userData);
		OutHit.HitActor     = OutHit.HitComponent->GetOwner();
	}
	else if (Block.actor && Block.actor->userData)
	{
		OutHit.HitActor = static_cast<AActor*>(Block.actor->userData);
	}
	return true;
}

// ============================================================
// SphereSweep (Start + End 시그니처)
// ============================================================

bool FPhysXPhysicsScene::SphereSweep(const FVector& Start, const FVector& End, float Radius,
	FHitResult& OutHit, ECollisionChannel TraceChannel, const AActor* IgnoreActor) const
{
	if (!Scene) return false;

	FVector Move = End - Start;
	float MoveDist2 = Move.X*Move.X + Move.Y*Move.Y + Move.Z*Move.Z;
	if (MoveDist2 < 1e-12f) return false;
	const float MoveDist = sqrtf(MoveDist2);
	const FVector Dir(Move.X/MoveDist, Move.Y/MoveDist, Move.Z/MoveDist);

	struct FChannelSweepFilter : PxQueryFilterCallback
	{
		const AActor* IgnoreActor = nullptr;
		PxU32 TraceBit = 0;
		FChannelSweepFilter(const AActor* A, ECollisionChannel Ch)
			: IgnoreActor(A), TraceBit(1u << static_cast<PxU32>(Ch)) {}
		PxQueryHitType::Enum preFilter(const PxFilterData&, const PxShape* Shape,
			const PxRigidActor* Actor, PxHitFlags&) override
		{
			if (IgnoreActor && Actor && Actor->userData == IgnoreActor) return PxQueryHitType::eNONE;
			if (Shape && (Shape->getQueryFilterData().word1 & TraceBit) == 0) return PxQueryHitType::eNONE;
			return PxQueryHitType::eBLOCK;
		}
		PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&) override
		{ return PxQueryHitType::eBLOCK; }
	};

	PxSphereGeometry SphereGeom(Radius);
	PxTransform StartPose(ToPxVec3(Start));
	PxSweepBuffer SweepHit;
	PxQueryFilterData FilterData;
	FilterData.flags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER;
	FChannelSweepFilter FilterCallback(IgnoreActor, TraceChannel);

	SCOPED_PHYSX_SCENE_READ_LOCK(Scene);
	bool bStatus = Scene->sweep(SphereGeom, StartPose, ToPxVec3(Dir), MoveDist,
		SweepHit, PxHitFlag::eDEFAULT | PxHitFlag::ePOSITION | PxHitFlag::eNORMAL,
		FilterData, &FilterCallback);
	if (!bStatus || !SweepHit.hasBlock) return false;

	const PxSweepHit& Block = SweepHit.block;
	const FVector ImpactNormal = ToFVector(Block.normal);
	OutHit.bHit             = true;
	OutHit.Distance         = Block.distance;
	OutHit.WorldHitLocation = ToFVector(Block.position);
	OutHit.ShapeLocation    = OutHit.WorldHitLocation + ImpactNormal * Radius;
	OutHit.ImpactNormal     = ImpactNormal;
	OutHit.WorldNormal      = ImpactNormal;

	if (Block.shape && Block.shape->userData)
	{
		OutHit.HitComponent = static_cast<UPrimitiveComponent*>(Block.shape->userData);
		OutHit.HitActor     = OutHit.HitComponent->GetOwner();
	}
	else if (Block.actor && Block.actor->userData)
	{
		OutHit.HitActor = static_cast<AActor*>(Block.actor->userData);
	}
	return true;
}

PxQueryHitType::Enum WheelRaycastPreFilter
(PxFilterData filterData0, PxFilterData filterData1,
	const void* constantBlock, PxU32 constantBlockSize,
	PxHitFlags& queryFlags)
{
	//filterData0 is the vehicle suspension raycast.
	//filterData1 is the shape potentially hit by the raycast.
	PX_UNUSED(constantBlockSize);
	PX_UNUSED(constantBlock);
	PX_UNUSED(queryFlags);
	if (filterData0.word2 != 0 && filterData0.word2 == filterData1.word2)
		return PxQueryHitType::eNONE;

	if ((filterData0.word3 & DRIVABLE_SURFACE) == 0)
		return PxQueryHitType::eNONE;

	return ((0 == (filterData1.word3 & DRIVABLE_SURFACE)) ?
		PxQueryHitType::eNONE : PxQueryHitType::eBLOCK);
}

FVehicleRuntimeHandle FPhysXPhysicsScene::CreateVehicle(const FVehicleRuntimeCreateDesc& CreateDesc)
{
	FVehicleRuntimeHandle Handle;
	if (!Scene || !Physics || !DefaultMaterial || !CreateDesc.IsValid())
	{
		return Handle;
	}

	const FVehicleBuildDesc& BuildDesc = CreateDesc.BuildDesc;
	const FVehicleDriveDesc& DriveDesc = BuildDesc.DriveDesc;
	const uint32              NumWheels = (uint32)BuildDesc.WheelDescs.size();
	if (NumWheels != GMaxNumWheels || DriveDesc.ChassisMass <= 0.0f)
	{
		return Handle;
	}

	CreateDesc.ChassisComponent->SetAutoCreatePhysicsBody(false);
	for (USceneComponent* WheelVisualComponent : CreateDesc.WheelVisualComponents)
	{
		if (auto* WheelPrimitive = Cast<UPrimitiveComponent>(WheelVisualComponent))
		{
			WheelPrimitive->SetAutoCreatePhysicsBody(false);
		}
	}

	FPhysXVehicleInstance* Instance = new FPhysXVehicleInstance();
	Instance->ChassisComponent = CreateDesc.ChassisComponent;
	Instance->SetWheelVisualComponents(CreateDesc.WheelVisualComponents);
	Instance->VehicleQueryResult = { Instance->WheelQueryResults, NumWheels };
	
	// -------------------------------------------------------
	// 1. WheelsSimData. setupWheelsSimulationData
	// -------------------------------------------------------
	PxVehicleWheelsSimData* WheelsSimData = PxVehicleWheelsSimData::allocate(NumWheels);
	auto FailCreateVehicle = [&]() -> FVehicleRuntimeHandle
	{
		if (WheelsSimData)
		{
			WheelsSimData->free();
			WheelsSimData = nullptr;
		}
		if (Instance)
		{
			Instance->Release();
			delete Instance;
			Instance = nullptr;
		}
		return Handle;
	};
	if (!WheelsSimData)
	{
		return FailCreateVehicle();
	}

	// 바퀴 위치 수집 (sprung mass 계산용)
	PxVec3 WheelCenterActorOffsets[GMaxNumWheels];
	for (uint32 i = 0; i < NumWheels; i++)
	{
		const FVehicleWheelDesc& WheelDesc = BuildDesc.WheelDescs[i];
		if (WheelDesc.Radius <= 0.0f || WheelDesc.Width <= 0.0f || WheelDesc.Mass <= 0.0f)
		{
			return FailCreateVehicle();
		}
		WheelCenterActorOffsets[i] = PxVec3(WheelDesc.LocalPosition.X, WheelDesc.LocalPosition.Y, WheelDesc.LocalPosition.Z);
	}

	// chassis center of mass offset (일단 원점)
	const PxVec3 ChassisCMOffset = ToPxVec3(DriveDesc.CenterOfMassOffset);

	// Sprung mass 계산
	PxF32 SprungMasses[GMaxNumWheels];
	PxVehicleComputeSprungMasses(NumWheels, WheelCenterActorOffsets,
		ChassisCMOffset, DriveDesc.ChassisMass, 2, SprungMasses);

	for (uint32 i = 0; i < NumWheels; i++)
	{
		const FVehicleWheelDesc& WheelDesc = BuildDesc.WheelDescs[i];
		const FVehicleSuspensionDesc& SuspensionDesc = WheelDesc.Suspension;

		// Wheel
		PxVehicleWheelData Wheel;
		Wheel.mRadius = WheelDesc.Radius;
		Wheel.mWidth = WheelDesc.Width;
		Wheel.mMass = WheelDesc.Mass;
		Wheel.mMOI = 0.5f * Wheel.mMass * Wheel.mRadius * Wheel.mRadius;
		Wheel.mDampingRate = 0.25f;
		Wheel.mMaxBrakeTorque = DriveDesc.MaxBrakeTorque;
		Wheel.mMaxHandBrakeTorque =
			(WheelDesc.WheelRole == EVehicleWheelRole::VWR_None || WheelDesc.WheelRole == EVehicleWheelRole::VWR_Drive)
			? DriveDesc.HandbrakeTorque : 0.f;
		Wheel.mMaxSteer =
			(WheelDesc.WheelRole == EVehicleWheelRole::VWR_Steering || WheelDesc.WheelRole == EVehicleWheelRole::VWR_SteerDrive)
			? PxPi * DriveDesc.MaxSteerAngle / 180.f : 0.f;

		// Tire
		PxVehicleTireData Tire;
		Tire.mType = 0; // TIRE_TYPE_NORMAL

		// Suspension
		PxVehicleSuspensionData Susp;
		Susp.mMaxCompression = SuspensionDesc.MaxCompression;
		Susp.mMaxDroop = SuspensionDesc.MaxDroop;
		Susp.mSpringStrength = SuspensionDesc.Stiffness;
		Susp.mSpringDamperRate = SuspensionDesc.Damping;
		Susp.mSprungMass = SprungMasses[i];

		// 오프셋
		PxVec3 WheelCMOffset = WheelCenterActorOffsets[i] - ChassisCMOffset;
		PxVec3 SuspForceOffset = PxVec3(WheelCMOffset.x, WheelCMOffset.y, -0.3f);
		PxVec3 TireForceOffset = PxVec3(WheelCMOffset.x, WheelCMOffset.y, -0.3f);

		WheelsSimData->setWheelData(i, Wheel);
		WheelsSimData->setTireData(i, Tire);
		WheelsSimData->setSuspensionData(i, Susp);
		WheelsSimData->setSuspTravelDirection(i, PxVec3(0.f, 0.f, -1.f)); // Z-up이므로 -Z
		WheelsSimData->setWheelCentreOffset(i, WheelCMOffset);
		WheelsSimData->setSuspForceAppPointOffset(i, SuspForceOffset);
		WheelsSimData->setTireForceAppPointOffset(i, TireForceOffset);
		WheelsSimData->setWheelShapeMapping(i, -1); // 1차: wheel shape 없음
	}

	// scene query filter
	PxFilterData QryFilterData;
	QryFilterData.word2 = CreateDesc.ChassisComponent->GetOwner()
		? CreateDesc.ChassisComponent->GetOwner()->GetUUID()
		: 0;
	QryFilterData.word3 = DRIVABLE_SURFACE;
	for (uint32 i = 0; i < NumWheels; i++)
		WheelsSimData->setSceneQueryFilterData(i, QryFilterData);

	SetComponentVehicleDrivableSurface(CreateDesc.ChassisComponent, false);
	for (USceneComponent* WheelVisualComponent : CreateDesc.WheelVisualComponents)
	{
		if (auto* WheelPrimitive = Cast<UPrimitiveComponent>(WheelVisualComponent))
			SetComponentVehicleDrivableSurface(WheelPrimitive, false);
	}

	// -------------------------------------------------------
	// 2. DriveSimData
	// -------------------------------------------------------
	PxVehicleDriveSimData4W DriveSimData;

	// Differential
	PxVehicleDifferential4WData Diff;
	switch (DriveDesc.DriveType)
	{
	case EVehicleDriveType::VDT_FrontWheel:
		Diff.mType = PxVehicleDifferential4WData::eDIFF_TYPE_LS_FRONTWD; break;
	case EVehicleDriveType::VDT_RearWheel:
		Diff.mType = PxVehicleDifferential4WData::eDIFF_TYPE_LS_REARWD;  break;
	case EVehicleDriveType::VDT_FourWheel:
	default:
		Diff.mType = PxVehicleDifferential4WData::eDIFF_TYPE_LS_4WD;     break;
	}
	DriveSimData.setDiffData(Diff);

	// Engine
	PxVehicleEngineData Engine;
	Engine.mPeakTorque = DriveDesc.MaxEngineTorque;
	Engine.mMaxOmega = DriveDesc.MaxEngineOmega; // omega = rpm * 2π / 60
	DriveSimData.setEngineData(Engine);

	// Gears
	PxVehicleGearsData Gears;
	Gears.mSwitchTime = 0.5f;
	DriveSimData.setGearsData(Gears);

	// Clutch
	PxVehicleClutchData Clutch;
	Clutch.mStrength = DriveDesc.ClutchStrength;
	DriveSimData.setClutchData(Clutch);

	// Ackermann
	const auto AbsFloat = [](float Value) { return Value < 0.0f ? -Value : Value; };
	const PxVec3 FrontLeftOffset = WheelsSimData->getWheelCentreOffset(PxVehicleDrive4WWheelOrder::eFRONT_LEFT);
	const PxVec3 FrontRightOffset = WheelsSimData->getWheelCentreOffset(PxVehicleDrive4WWheelOrder::eFRONT_RIGHT);
	const PxVec3 RearLeftOffset = WheelsSimData->getWheelCentreOffset(PxVehicleDrive4WWheelOrder::eREAR_LEFT);
	const PxVec3 RearRightOffset = WheelsSimData->getWheelCentreOffset(PxVehicleDrive4WWheelOrder::eREAR_RIGHT);

	PxVehicleAckermannGeometryData Ackermann;
	Ackermann.mAccuracy = 1.f;
	Ackermann.mAxleSeparation = AbsFloat(FrontLeftOffset.x - RearLeftOffset.x);
	Ackermann.mFrontWidth = AbsFloat(FrontRightOffset.y - FrontLeftOffset.y);
	Ackermann.mRearWidth = AbsFloat(RearRightOffset.y - RearLeftOffset.y);
	if (Ackermann.mAxleSeparation <= 0.0f || Ackermann.mFrontWidth <= 0.0f || Ackermann.mRearWidth <= 0.0f)
	{
		return FailCreateVehicle();
	}
	DriveSimData.setAckermannGeometryData(Ackermann);

	// -------------------------------------------------------
	// 3. Chassis Actor
	// -------------------------------------------------------

	Instance->ChassisActor = Physics->createRigidDynamic(GetPxTransform(CreateDesc.ChassisComponent));
	if (!Instance->ChassisActor)
	{
		return FailCreateVehicle();
	}
	Instance->ChassisActor->userData = CreateDesc.ChassisComponent->GetOwner();

	// chassis convex mesh가 있으면 shape 추가, 없으면 box로 대체
	PxShape* ChassisShape = nullptr;
	{
		FVector ChassisExtent(1.0f, 0.5f, 2.0f);
		if (auto* Box = Cast<UBoxComponent>(CreateDesc.ChassisComponent))
		{
			ChassisExtent = Box->GetScaledBoxExtent();
		}
		else
		{
			const FBoundingBox Bounds = CreateDesc.ChassisComponent->GetWorldBoundingBox();
			if (Bounds.IsValid())
				ChassisExtent = Bounds.GetExtent();
		}
		if (ChassisExtent.X <= 0.0f || ChassisExtent.Y <= 0.0f || ChassisExtent.Z <= 0.0f)
		{
			return FailCreateVehicle();
		}

		PxBoxGeometry BoxGeom(ChassisExtent.X, ChassisExtent.Y, ChassisExtent.Z);
		ChassisShape = PxRigidActorExt::createExclusiveShape(
			*Instance->ChassisActor, BoxGeom, *DefaultMaterial);
		if (!ChassisShape)
		{
			return FailCreateVehicle();
		}

		SetupFilterData(ChassisShape, CreateDesc.ChassisComponent);
		SetupVehicleQueryFilterData(ChassisShape, CreateDesc.ChassisComponent, false);
		ChassisShape->userData = CreateDesc.ChassisComponent;
	}

	PxRigidBodyExt::setMassAndUpdateInertia(*Instance->ChassisActor, DriveDesc.ChassisMass, &ChassisCMOffset);
	Instance->ChassisActor->setCMassLocalPose(PxTransform(ChassisCMOffset));

	// -------------------------------------------------------
	// 4. PxVehicleDrive4W 조립
	// -------------------------------------------------------
	Instance->Vehicle = PxVehicleDrive4W::allocate(NumWheels);
	if (!Instance->Vehicle)
	{
		return FailCreateVehicle();
	}
	Instance->Vehicle->setup(Physics, Instance->ChassisActor,
		*WheelsSimData, DriveSimData, 0);
	Instance->Vehicle->setToRestState();
	Instance->Vehicle->mDriveDynData.setUseAutoGears(true);
	WheelsSimData->free();
	WheelsSimData = nullptr;

	// -------------------------------------------------------
	// 5. FrictionPairs
	// -------------------------------------------------------
	Instance->FrictionPairs =
		PxVehicleDrivableSurfaceToTireFrictionPairs::allocate(1, 1);
	if (!Instance->FrictionPairs)
	{
		return FailCreateVehicle();
	}

	PxVehicleDrivableSurfaceType SurfaceType;
	SurfaceType.mType = 0; // SURFACE_TYPE_TARMAC

	const PxMaterial* SurfaceMaterials[1] = { DefaultMaterial };
	Instance->FrictionPairs->setup(1, 1, SurfaceMaterials, &SurfaceType);
	Instance->FrictionPairs->setTypePairFriction(0, 0, 1.f);

	// -------------------------------------------------------
	// 6. BatchQuery
	// -------------------------------------------------------
	PxBatchQueryDesc BqDesc(NumWheels, 0, 0);
	BqDesc.queryMemory.userRaycastResultBuffer = Instance->SqResults;
	BqDesc.queryMemory.userRaycastTouchBuffer = Instance->SqHitBuffer;
	BqDesc.queryMemory.raycastTouchBufferSize = NumWheels;
	BqDesc.preFilterShader = WheelRaycastPreFilter;
	Instance->BatchQuery = Scene->createBatchQuery(BqDesc);
	if (!Instance->BatchQuery)
	{
		return FailCreateVehicle();
	}

	// -------------------------------------------------------
	// 7. Scene에 추가 및 Handle 반환
	// -------------------------------------------------------
	Scene->addActor(*Instance->ChassisActor);
	VehicleInstances.push_back(Instance);

	Handle.NativeVehicle = Instance;
	return Handle;
}

void FPhysXPhysicsScene::DestroyVehicle(FVehicleRuntimeHandle& Handle)
{
	if (!Handle.IsValid())
		return;

	FPhysXVehicleInstance* Instance = static_cast<FPhysXVehicleInstance*>(Handle.NativeVehicle);
	if (!Instance)
		return;

	// VehicleInstances 목록에서 제거
	auto It = std::find(VehicleInstances.begin(), VehicleInstances.end(), Instance);
	if (It != VehicleInstances.end())
		VehicleInstances.erase(It);

	// Scene에서 actor 제거
	if (Instance->ChassisActor && Scene)
		Scene->removeActor(*Instance->ChassisActor);

	// PhysX 객체 해제
	Instance->Release();

	delete Instance;
	Handle.Reset();
}

void FPhysXPhysicsScene::UpdateVehicles(float DeltaTime)
{
	if (VehicleInstances.empty())
	{
		return;
	}

	const PxVec3 Gravity = Scene ? Scene->getGravity() : PxVec3(0, 0, -9.81f);
	for (FPhysXVehicleInstance* Instance : VehicleInstances)
	{
		if (!Instance || !Instance->Vehicle || !Instance->BatchQuery || !Instance->FrictionPairs)
		{
			continue;
		}

		PxVehicleWheels* Vehicles[1] = { Instance->Vehicle };
		PxVehicleSuspensionRaycasts(Instance->BatchQuery, 1, Vehicles, 4, Instance->SqResults);

		const float ForwardSpeed = Instance->Vehicle->computeForwardSpeed();
		const float ShiftSpeed = 0.5f;
		const float RawThrottle = Instance->InputState.Throttle;
		const float RawBrake = Instance->InputState.Brake;
		float AppliedAccel = 0.0f;
		float AppliedBrake = 0.0f;

		if (RawThrottle > 0.0f && ForwardSpeed >= -ShiftSpeed)
		{
			const PxU32 TargetGear = Instance->Vehicle->mDriveDynData.getTargetGear();

			if (TargetGear == PxVehicleGearsData::eREVERSE ||
				TargetGear == PxVehicleGearsData::eNEUTRAL)
			{
				Instance->Vehicle->mDriveDynData.setTargetGear(PxVehicleGearsData::eFIRST);
			}

			AppliedAccel = RawThrottle;
		}
		else if (RawBrake > 0.0f)
		{
			if (ForwardSpeed > ShiftSpeed)
			{
				AppliedBrake = RawBrake;
			}
			else
			{
				const PxU32 TargetGear = Instance->Vehicle->mDriveDynData.getTargetGear();
				if (TargetGear != PxVehicleGearsData::eREVERSE)
				{
					Instance->Vehicle->mDriveDynData.setTargetGear(PxVehicleGearsData::eREVERSE);
				}
				AppliedAccel = RawBrake;
			}
		}

		PxVehicleDrive4WRawInputData RawInputData;
		RawInputData.setAnalogAccel(AppliedAccel);
		RawInputData.setAnalogBrake(AppliedBrake);
		RawInputData.setAnalogHandbrake(Instance->InputState.bHandbrake ? 1.0f : 0.0f);
		RawInputData.setAnalogSteer(Instance->InputState.Steering);

		const bool bVehicleWasInAir = Instance->DebugStats.WheelCount > 0 ? Instance->DebugStats.bInAir : false;
		PxVehicleDrive4WSmoothAnalogRawInputsAndSetAnalogInputs(
			GetVehiclePadSmoothingData(),
			GetVehicleSteerVsForwardSpeedTable(),
			RawInputData,
			DeltaTime,
			bVehicleWasInAir,
			*Instance->Vehicle);

		PxVehicleUpdates(DeltaTime, Gravity, *Instance->FrictionPairs, 1, Vehicles, &Instance->VehicleQueryResult);

		Instance->DebugStats.WheelCount = GMaxNumWheels;
		Instance->DebugStats.CurrentSpeed = Instance->Vehicle->computeForwardSpeed() * 3.6f;
		Instance->DebugStats.EngineRpm = Instance->Vehicle->mDriveDynData.getEngineRotationSpeed() * 60.0f / 6.28318530718f;
		Instance->DebugStats.InputState = Instance->InputState;
		Instance->DebugStats.EngineTorque = Instance->Vehicle->mDriveDynData.getAnalogInput(PxVehicleDrive4WControl::eANALOG_INPUT_ACCEL);
		Instance->DebugStats.BrakeTorque = Instance->Vehicle->mDriveDynData.getAnalogInput(PxVehicleDrive4WControl::eANALOG_INPUT_BRAKE);
		Instance->DebugStats.SteeringAngle =
			Instance->Vehicle->mDriveDynData.getAnalogInput(PxVehicleDrive4WControl::eANALOG_INPUT_STEER_RIGHT)
			- Instance->Vehicle->mDriveDynData.getAnalogInput(PxVehicleDrive4WControl::eANALOG_INPUT_STEER_LEFT);
		Instance->DebugStats.bInAir = true;
		for (uint32 i = 0; i < GMaxNumWheels; ++i)
		{
			if (!Instance->WheelQueryResults[i].isInAir)
			{
				Instance->DebugStats.bInAir = false;
				break;
			}
		}
	}
}

void FPhysXPhysicsScene::SyncVehiclePose()
{
	for (FPhysXVehicleInstance* Instance : VehicleInstances)
	{
		if (!Instance || !Instance->Vehicle || !Instance->ChassisActor || !Instance->ChassisComponent)
		{
			continue;
		}

		PxTransform ChassisPose = Instance->ChassisActor->getGlobalPose();
		Instance->ChassisComponent->SetWorldLocation(ToFVector(ChassisPose.p));
		SetComponentWorldRotation(Instance->ChassisComponent, ToFQuat(ChassisPose.q));

		Instance->DebugStats.CenterOfMassWorld = ToFVector(ChassisPose.transform(Instance->ChassisActor->getCMassLocalPose().p));
		Instance->DebugStats.Wheels.resize(GMaxNumWheels);

		for (uint32 i = 0; i < GMaxNumWheels; i++)
		{
			const PxWheelQueryResult& WheelResult = Instance->WheelQueryResults[i];
			PxTransform WheelWorldPose = ChassisPose * WheelResult.localPose;
			FVehicleWheelDebugState& WheelDebug = Instance->DebugStats.Wheels[i];
			WheelDebug.SuspensionStart = ToFVector(WheelResult.suspLineStart);
			WheelDebug.SuspensionEnd = ToFVector(WheelResult.suspLineStart + WheelResult.suspLineDir * WheelResult.suspLineLength);
			WheelDebug.ContactPoint = ToFVector(WheelResult.tireContactPoint);
			WheelDebug.ContactNormal = ToFVector(WheelResult.tireContactNormal);
			WheelDebug.WheelCenter = ToFVector(WheelWorldPose.p);
			WheelDebug.bInAir = WheelResult.isInAir;

			if (i >= (uint32)Instance->WheelVisualComponents.size()) continue;
			USceneComponent* WheelComp = Instance->WheelVisualComponents[i];
			if (!WheelComp) continue;

			if (WheelComp->GetParent() == Instance->ChassisComponent)
			{
				const FQuat WheelPose = ToFQuat(WheelResult.localPose.q);
				const FQuat VisualOffset = Instance->GetWheelVisualRotationOffset(i);

				WheelComp->SetRelativeRotation(WheelPose * VisualOffset);
				WheelComp->SetRelativeLocation(ToFVector(WheelResult.localPose.p));
			}
			else
			{
				const FQuat WheelPose = ToFQuat(WheelWorldPose.q);
				const FQuat VisualOffset = Instance->GetWheelVisualRotationOffset(i);

				WheelComp->SetWorldLocation(ToFVector(WheelWorldPose.p));
				SetComponentWorldRotation(WheelComp, WheelPose * VisualOffset);
			}
		}
	}
}
