#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"

class IPhysicsSceneInterface;
class UPhysicsAsset;
class USkeletalMeshComponent;
class FPhysicsBodyInstance;
class FPhysicsConstraintInstance;

/** PhysicsAsset을 런타임 PhysX body/constraint 묶음으로 생성하고 SkeletalMesh pose에 동기화한다. */
struct FRagdollInstance
{
    bool Initialize(UPhysicsAsset* PhysicsAsset, USkeletalMeshComponent* MeshComp, IPhysicsSceneInterface* Scene);
    void RestoreInitialPose(USkeletalMeshComponent* MeshComp) const;
    void Release(IPhysicsSceneInterface* Scene);
    void SyncBonesFromBodies(USkeletalMeshComponent* MeshComp, IPhysicsSceneInterface* Scene);

    bool IsActive() const { return bInitialized && !Bodies.empty(); }

private:
    TArray<FPhysicsBodyInstance*> Bodies;
    TArray<int32> BodyToBoneIndex;
    TArray<FPhysicsConstraintInstance*> Constraints;
    TArray<FMatrix> InitialLocalPose;
    FVector ComponentWorldScaleAtStart = FVector::OneVector;
    bool bInitialized = false;
};

