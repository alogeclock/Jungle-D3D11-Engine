#include "Physics/Systems/Ragdoll/RagdollInstance.h"

#include "Component/SkeletalMeshComponent.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletonAsset.h"
#include "Physics/Assets/PhysicsAsset.h"
#include "Physics/Assets/PhysicsBodySetup.h"
#include "Physics/Assets/PhysicsConstraintSetup.h"
#include "Physics/Runtime/PhysicsBodyInstance.h"
#include "Physics/Runtime/PhysicsConstraintInstance.h"
#include "Physics/Runtime/PhysicsSceneInterface.h"

#include <algorithm>
#include <cmath>

static FVector AbsVector(const FVector& V)
{
    return FVector(std::abs(V.X), std::abs(V.Y), std::abs(V.Z));
}

static FVector MultiplyVectorComponents(const FVector& A, const FVector& B)
{
    return FVector(A.X * B.X, A.Y * B.Y, A.Z * B.Z);
}

static float MaxComponent(const FVector& V)
{
    return (std::max)(V.X, (std::max)(V.Y, V.Z));
}

static void ScaleBodyDescForComponent(FPhysicsBodyDesc& BodyDesc, const FVector& ComponentScale)
{
    const FVector AbsScale = AbsVector(ComponentScale);
    const float UniformScale = (std::max)(0.01f, MaxComponent(AbsScale));

    for (FPhysicsShapeDesc& ShapeDesc : BodyDesc.Shapes)
    {
        ShapeDesc.LocalTransform.Location = MultiplyVectorComponents(ShapeDesc.LocalTransform.Location, ComponentScale);

        switch (ShapeDesc.ShapeType)
        {
        case EPhysicsShapeType::PST_Sphere:
            ShapeDesc.Size.X *= UniformScale;
            ShapeDesc.Size.Y = ShapeDesc.Size.X;
            ShapeDesc.Size.Z = ShapeDesc.Size.X;
            break;
        case EPhysicsShapeType::PST_Box:
            ShapeDesc.Size = MultiplyVectorComponents(ShapeDesc.Size, AbsScale);
            break;
        case EPhysicsShapeType::PST_Capsule:
            ShapeDesc.Size.X *= (std::max)(0.01f, (std::max)(AbsScale.Y, AbsScale.Z));
            ShapeDesc.Size.Y *= (std::max)(0.01f, AbsScale.X);
            break;
        case EPhysicsShapeType::PST_Convex:
            for (FVector& Vertex : ShapeDesc.VertexData)
            {
                Vertex = MultiplyVectorComponents(Vertex, ComponentScale);
            }
            break;
        }
    }
}

static void ScaleConstraintDescForComponent(FPhysicsConstraintDesc& ConstraintDesc, const FVector& ComponentScale)
{
    ConstraintDesc.ParentLocalFrame.Location =
        MultiplyVectorComponents(ConstraintDesc.ParentLocalFrame.Location, ComponentScale);
    ConstraintDesc.ChildLocalFrame.Location =
        MultiplyVectorComponents(ConstraintDesc.ChildLocalFrame.Location, ComponentScale);
    ConstraintDesc.LinearLimit *= (std::max)(0.01f, MaxComponent(AbsVector(ComponentScale)));
}

static bool GetBoneWorldTransform(USkeletalMeshComponent* MeshComp, int32 BoneIndex, FTransform& OutWorldTransform)
{
    if (!MeshComp)
    {
        return false;
    }

    TArray<FMatrix> ComponentBoneGlobals;
    MeshComp->GetCurrentBoneGlobalMatrices(ComponentBoneGlobals);
    if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(ComponentBoneGlobals.size()))
    {
        return false;
    }

    const FMatrix BoneWorldMatrix = ComponentBoneGlobals[BoneIndex] * MeshComp->GetWorldMatrix();
    OutWorldTransform = FTransform(BoneWorldMatrix);
    OutWorldTransform.Scale = FVector::OneVector;
    return true;
}

bool FRagdollInstance::Initialize(
    UPhysicsAsset* PhysicsAsset,
    USkeletalMeshComponent* MeshComp,
    IPhysicsSceneInterface* Scene)
{
    Release(Scene);

    if (!PhysicsAsset || !MeshComp || !Scene || !MeshComp->GetSkeletalMesh())
    {
        return false;
    }

    const FSkeletonAsset* SkeletonAsset = MeshComp->GetSkeletalMesh()->GetSkeletonAsset();
    if (!SkeletonAsset || SkeletonAsset->Bones.empty())
    {
        return false;
    }

    const int32 BoneCount = static_cast<int32>(SkeletonAsset->Bones.size());
    InitialLocalPose.clear();
    InitialLocalPose.reserve(BoneCount);
    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        InitialLocalPose.push_back(MeshComp->GetBoneLocalTransformByIndex(BoneIndex).ToMatrix());
    }

    ComponentWorldScaleAtStart = MeshComp->GetWorldScale();

    TMap<FString, int32> BoneNameToBodyIndex;
    const TArray<UPhysicsBodySetup*>& BodySetups = PhysicsAsset->GetBodySetups();
    Bodies.reserve(BodySetups.size());
    BodyToBoneIndex.reserve(BodySetups.size());

    for (UPhysicsBodySetup* BodySetup : BodySetups)
    {
        if (!BodySetup)
        {
            continue;
        }

        const FString BoneName = BodySetup->GetTargetBoneName().ToString();
        const int32 BoneIndex = MeshComp->FindBoneIndexByName(BoneName);
        if (BoneIndex == INDEX_NONE)
        {
            continue;
        }

        FTransform BoneWorldTransform;
        if (!GetBoneWorldTransform(MeshComp, BoneIndex, BoneWorldTransform))
        {
            continue;
        }

        FPhysicsBodyDesc BodyDesc = BodySetup->BuildBodyDesc();
        BodyDesc.BodyType = EPhysicsBodyType::PBT_Dynamic;
        ScaleBodyDescForComponent(BodyDesc, ComponentWorldScaleAtStart);

        FPhysicsBodyInstance* BodyInstance =
            Scene->CreateBodyAtTransform(MeshComp, BodyDesc, BoneWorldTransform, false);
        if (!BodyInstance)
        {
            continue;
        }

        const int32 BodyIndex = static_cast<int32>(Bodies.size());
        Bodies.push_back(BodyInstance);
        BodyToBoneIndex.push_back(BoneIndex);
        BoneNameToBodyIndex[BoneName] = BodyIndex;
    }

    if (Bodies.empty())
    {
        Release(Scene);
        return false;
    }

    const TArray<FPhysicsConstraintSetup>& ConstraintSetups = PhysicsAsset->GetConstraintSetups();
    Constraints.reserve(ConstraintSetups.size());
    for (const FPhysicsConstraintSetup& ConstraintSetup : ConstraintSetups)
    {
        const auto ParentBodyIt = BoneNameToBodyIndex.find(ConstraintSetup.ParentBoneName.ToString());
        const auto ChildBodyIt  = BoneNameToBodyIndex.find(ConstraintSetup.ChildBoneName.ToString());
        if (ParentBodyIt == BoneNameToBodyIndex.end() || ChildBodyIt == BoneNameToBodyIndex.end())
        {
            continue;
        }

        FPhysicsConstraintDesc ConstraintDesc = ConstraintSetup.BuildConstraintDesc();
        ScaleConstraintDescForComponent(ConstraintDesc, ComponentWorldScaleAtStart);

        FPhysicsConstraintInstance* ConstraintInstance = Scene->CreateConstraint(
            Bodies[ParentBodyIt->second],
            Bodies[ChildBodyIt->second],
            ConstraintDesc);
        if (ConstraintInstance)
        {
            Constraints.push_back(ConstraintInstance);
        }
    }

    bInitialized = true;
    return true;
}

void FRagdollInstance::RestoreInitialPose(USkeletalMeshComponent* MeshComp) const
{
    if (!MeshComp || InitialLocalPose.empty())
    {
        return;
    }

    MeshComp->ApplyPhysicsBoneLocalMatrices(InitialLocalPose);
}

void FRagdollInstance::Release(IPhysicsSceneInterface* Scene)
{
    if (Scene)
    {
        for (FPhysicsConstraintInstance* ConstraintInstance : Constraints)
        {
            Scene->DestroyConstraint(ConstraintInstance);
        }
        for (FPhysicsBodyInstance* BodyInstance : Bodies)
        {
            Scene->DestroyBody(BodyInstance);
        }
    }

    Constraints.clear();
    Bodies.clear();
    BodyToBoneIndex.clear();
    InitialLocalPose.clear();
    ComponentWorldScaleAtStart = FVector::OneVector;
    bInitialized = false;
}

void FRagdollInstance::SyncBonesFromBodies(USkeletalMeshComponent* MeshComp, IPhysicsSceneInterface* Scene)
{
    if (!IsActive() || !MeshComp || !Scene || !MeshComp->GetSkeletalMesh())
    {
        return;
    }

    const FSkeletonAsset* SkeletonAsset = MeshComp->GetSkeletalMesh()->GetSkeletonAsset();
    if (!SkeletonAsset)
    {
        return;
    }

    const int32 BoneCount = static_cast<int32>(SkeletonAsset->Bones.size());
    if (BoneCount <= 0 || static_cast<int32>(InitialLocalPose.size()) != BoneCount)
    {
        return;
    }

    TArray<FMatrix> ComponentGlobals;
    ComponentGlobals.resize(BoneCount, FMatrix::Identity);
    TArray<bool> bSolved;
    bSolved.resize(BoneCount, false);

    const FMatrix ComponentWorld = MeshComp->GetWorldMatrix();
    const FMatrix ComponentWorldInv = ComponentWorld.GetInverse();
    const FTransform ComponentWorldNoScale(
        MeshComp->GetWorldLocation(),
        ComponentWorld.ToQuat(),
        FVector::OneVector);
    const FMatrix ComponentWorldNoScaleInv = ComponentWorldNoScale.ToMatrix().GetInverse();

    for (int32 BodyIndex = 0; BodyIndex < static_cast<int32>(Bodies.size()); ++BodyIndex)
    {
        if (BodyIndex >= static_cast<int32>(BodyToBoneIndex.size()))
        {
            continue;
        }

        const int32 BoneIndex = BodyToBoneIndex[BodyIndex];
        if (BoneIndex < 0 || BoneIndex >= BoneCount)
        {
            continue;
        }

        FTransform BodyWorldTransform;
        if (!Scene->GetBodyWorldTransform(Bodies[BodyIndex], BodyWorldTransform))
        {
            continue;
        }

        FMatrix ComponentGlobal =
            FTransform(BodyWorldTransform.Location, BodyWorldTransform.Rotation, FVector::OneVector).ToMatrix()
            * ComponentWorldNoScaleInv;
        ComponentGlobal.SetLocation(ComponentWorldInv.TransformPositionWithW(BodyWorldTransform.Location));

        ComponentGlobals[BoneIndex] = ComponentGlobal;
        bSolved[BoneIndex] = true;
    }

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        if (bSolved[BoneIndex])
        {
            continue;
        }

        const int32 ParentIndex = SkeletonAsset->Bones[BoneIndex].ParentIndex;
        if (ParentIndex >= 0 && ParentIndex < BoneCount && bSolved[ParentIndex])
        {
            ComponentGlobals[BoneIndex] = InitialLocalPose[BoneIndex] * ComponentGlobals[ParentIndex];
            bSolved[BoneIndex] = true;
        }
    }

    bool bMadeProgress = true;
    while (bMadeProgress)
    {
        bMadeProgress = false;
        for (int32 BoneIndex = BoneCount - 1; BoneIndex >= 0; --BoneIndex)
        {
            if (bSolved[BoneIndex])
            {
                continue;
            }

            for (int32 ChildIndex = BoneIndex + 1; ChildIndex < BoneCount; ++ChildIndex)
            {
                if (SkeletonAsset->Bones[ChildIndex].ParentIndex == BoneIndex && bSolved[ChildIndex])
                {
                    ComponentGlobals[BoneIndex] =
                        InitialLocalPose[ChildIndex].GetInverse() * ComponentGlobals[ChildIndex];
                    bSolved[BoneIndex] = true;
                    bMadeProgress = true;
                    break;
                }
            }
        }
    }

    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        if (bSolved[BoneIndex])
        {
            continue;
        }

        const int32 ParentIndex = SkeletonAsset->Bones[BoneIndex].ParentIndex;
        if (ParentIndex >= 0 && ParentIndex < BoneCount)
        {
            ComponentGlobals[BoneIndex] = InitialLocalPose[BoneIndex] * ComponentGlobals[ParentIndex];
        }
        else
        {
            ComponentGlobals[BoneIndex] = InitialLocalPose[BoneIndex];
        }
        bSolved[BoneIndex] = true;
    }

    TArray<FMatrix> LocalPose;
    LocalPose.resize(BoneCount);
    for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
    {
        const int32 ParentIndex = SkeletonAsset->Bones[BoneIndex].ParentIndex;
        FMatrix LocalMatrix = (ParentIndex >= 0 && ParentIndex < BoneCount)
            ? ComponentGlobals[BoneIndex] * ComponentGlobals[ParentIndex].GetInverse()
            : ComponentGlobals[BoneIndex];

        FTransform LocalTransform(LocalMatrix);
        LocalTransform.Scale = FTransform(InitialLocalPose[BoneIndex]).Scale;
        LocalPose[BoneIndex] = LocalTransform.ToMatrix();
    }

    MeshComp->ApplyPhysicsBoneLocalMatrices(LocalPose);
}

