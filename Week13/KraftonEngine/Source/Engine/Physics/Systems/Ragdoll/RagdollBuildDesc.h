#pragma once

#include "RagdollTypes.h"
#include "Physics/Assets/PhysicsAsset.h"

class USkeletalMesh;
class USkeleton;

/** Ragdoll Runtime 생성에 필요한 공용 입력 데이터 */
struct FRagdollBuildDesc
{
    USkeletalMesh*  SkeletalMesh = nullptr;
    USkeleton*      Skeleton     = nullptr;
    UPhysicsAsset*  PhysicsAsset = nullptr;

    TArray<FPhysicsBodyDesc>       BodyDescs;
    TArray<FPhysicsConstraintDesc> ConstraintDescs;
    TArray<FRagdollBoneMapping>    BoneMappings;

    bool IsValid() const
    {
        return SkeletalMesh != nullptr &&
               Skeleton     != nullptr &&
               PhysicsAsset != nullptr &&
               !BodyDescs.empty();
    }
};
