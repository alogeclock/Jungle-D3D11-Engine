#include "Physics/Systems/Ragdoll/RagdollInstance.h"

#include "Component/SkeletalMeshComponent.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletonAsset.h"
#include "Physics/Assets/PhysicsAsset.h"
#include "Physics/Assets/PhysicsBodySetup.h"
#include "Physics/Assets/PhysicsConstraintSetup.h"
#include "Physics/Common/PhysicsConversionTypes.h"
#include "Physics/Runtime/PhysicsBodyInstance.h"
#include "Physics/Runtime/PhysicsConstraintInstance.h"
#include "Physics/Runtime/PhysicsSceneInterface.h"
#include "Component/SceneComponent.h"
#include "Core/Log.h"

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

static float QuatAngularDistanceDegrees(FQuat A, FQuat B)
{
	A.Normalize();
	B.Normalize();

	float Dot = std::abs(A.X * B.X + A.Y * B.Y + A.Z * B.Z + A.W * B.W);
	Dot = std::clamp(Dot, 0.0f, 1.0f);
	return std::acos(Dot) * 2.0f * 180.0f / 3.1415926535f;
}

static FTransform ComposeLocalWithWorld(const FTransform& LocalFrame, const FTransform& WorldFrame)
{
	FTransform Result(LocalFrame.ToMatrix() * WorldFrame.ToMatrix());
	Result.Scale = FVector::OneVector;
	return Result;
}

static void LogConstraintFrameError(
	const FPhysicsConstraintSetup& ConstraintSetup,
	const FPhysicsConstraintDesc& ConstraintDesc,
	FPhysicsBodyInstance* ParentBody,
	FPhysicsBodyInstance* ChildBody,
	IPhysicsSceneInterface* Scene)
{
	if (!Scene || !ParentBody || !ChildBody)
	{
		return;
	}

	FTransform ParentWorld;
	FTransform ChildWorld;
	if (!Scene->GetBodyWorldTransform(ParentBody, ParentWorld) ||
		!Scene->GetBodyWorldTransform(ChildBody, ChildWorld))
	{
		return;
	}

	const FTransform ParentJointWorld = ComposeLocalWithWorld(ConstraintDesc.ParentLocalFrame, ParentWorld);
	const FTransform ChildJointWorld = ComposeLocalWithWorld(ConstraintDesc.ChildLocalFrame, ChildWorld);
	const float PositionError = FVector::Distance(ParentJointWorld.Location, ChildJointWorld.Location);
	const float AngularError = QuatAngularDistanceDegrees(ParentJointWorld.Rotation, ChildJointWorld.Rotation);
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

static void BuildCompPose(const FSkeletonAsset* SkeletonAsset, const TArray<FMatrix>& LocalPose, TArray<FMatrix>& OutPose)
{
	OutPose.clear();
	if (!SkeletonAsset || LocalPose.empty())
	{
		return;
	}

	const int32 BoneCount = static_cast<int32>(SkeletonAsset->Bones.size());
	if (BoneCount <= 0 || static_cast<int32>(LocalPose.size()) != BoneCount)
	{
		return;
	}

	OutPose.resize(BoneCount, FMatrix::Identity);
	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		const int32 ParentIndex = SkeletonAsset->Bones[BoneIndex].ParentIndex;
		OutPose[BoneIndex] = (ParentIndex >= 0 && ParentIndex < BoneCount)
			? LocalPose[BoneIndex] * OutPose[ParentIndex]
			: LocalPose[BoneIndex];
	}
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
	BuildCompPose(SkeletonAsset, InitialLocalPose, InitialComponentPose);

	StartScale = MeshComp->GetWorldScale();

	TMap<FString, int32> BoneNameToBodyIndex;
	const TArray<UPhysicsBodySetup*>& BodySetups = PhysicsAsset->GetBodySetups();
	Bodies.reserve(BodySetups.size());
	BodyToBoneIndex.reserve(BodySetups.size());

	const EPhysicsAssetRagdollMode RequestedRagdollMode = PhysicsAsset->GetRagdollMode();
	const bool bRequestAggregate = RequestedRagdollMode == EPhysicsAssetRagdollMode::PxAggregate;
	if (bRequestAggregate && !Scene->SupportsAggregateRagdolls())
	{
		UE_LOG("[RagdollBuildModeFallback] Owner=%s RequestedMode=%s backend does not support aggregates. Falling back to Per-Body.",
			MeshComp->GetName().c_str(),
			PhysicsAssetRagdollModeToString(RequestedRagdollMode));
	}
	else if (bRequestAggregate && BodySetups.size() > 128)
	{
		UE_LOG("[RagdollBuildModeFallback] Owner=%s RequestedMode=%s BodyCount=%d exceeds PhysX aggregate capacity. Falling back to Per-Body.",
			MeshComp->GetName().c_str(),
			PhysicsAssetRagdollModeToString(RequestedRagdollMode),
			static_cast<int32>(BodySetups.size()));
	}
	else if (bRequestAggregate)
	{
		AggregateHandle = Scene->CreateAggregateHandle(static_cast<uint32>(BodySetups.size()), true);
		if (!AggregateHandle)
		{
			UE_LOG("[RagdollBuildModeFallback] Owner=%s RequestedMode=%s aggregate creation failed. Falling back to Per-Body.",
				MeshComp->GetName().c_str(),
				PhysicsAssetRagdollModeToString(RequestedRagdollMode));
		}
	}

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
		BodyDesc.bEnableSelfCollision = true;
		ScaleBodyDescForComponent(BodyDesc, StartScale);

		UE_LOG("[RagdollBuildBody] Owner=%s Bone=%s BoneIndex=%d ShapeCount=%d Mass=%.3f ComponentScale=(%.3f, %.3f, %.3f) BoneWorldP=(%.3f, %.3f, %.3f) BoneWorldQ=(%.3f, %.3f, %.3f, %.3f)",
			MeshComp->GetName().c_str(),
			BoneName.c_str(),
			BoneIndex,
			static_cast<int32>(BodyDesc.Shapes.size()),
			BodyDesc.Mass,
			StartScale.X, StartScale.Y, StartScale.Z,
			BoneWorldTransform.Location.X, BoneWorldTransform.Location.Y, BoneWorldTransform.Location.Z,
			BoneWorldTransform.Rotation.X, BoneWorldTransform.Rotation.Y, BoneWorldTransform.Rotation.Z, BoneWorldTransform.Rotation.W);

		for (int32 ShapeIndex = 0; ShapeIndex < static_cast<int32>(BodyDesc.Shapes.size()); ++ShapeIndex)
		{
			const FPhysicsShapeDesc& ShapeDesc = BodyDesc.Shapes[ShapeIndex];
			UE_LOG("[RagdollBuildShape] Bone=%s ShapeIndex=%d Type=%s Size=(%.3f, %.3f, %.3f) LocalP=(%.3f, %.3f, %.3f) LocalQ=(%.3f, %.3f, %.3f, %.3f)",
				BoneName.c_str(),
				ShapeIndex,
				ToShapeTypeDebugName(ShapeDesc.ShapeType),
				ShapeDesc.Size.X, ShapeDesc.Size.Y, ShapeDesc.Size.Z,
				ShapeDesc.LocalTransform.Location.X, ShapeDesc.LocalTransform.Location.Y, ShapeDesc.LocalTransform.Location.Z,
				ShapeDesc.LocalTransform.Rotation.X, ShapeDesc.LocalTransform.Rotation.Y,
				ShapeDesc.LocalTransform.Rotation.Z, ShapeDesc.LocalTransform.Rotation.W);
		}

		FPhysicsBodyInstance* BodyInstance = AggregateHandle
			? Scene->CreateBodyAtTransformInAggregate(MeshComp, BodyDesc, BoneWorldTransform, AggregateHandle, false)
			: Scene->CreateBodyAtTransform(MeshComp, BodyDesc, BoneWorldTransform, false);
		if (!BodyInstance)
		{
			UE_LOG("[RagdollBuildBodyError] Owner=%s Bone=%s failed to create runtime body",
				MeshComp->GetName().c_str(), BoneName.c_str());
			continue;
		}

		const int32 BodyIndex = static_cast<int32>(Bodies.size());
		Bodies.push_back(BodyInstance);
		BodyToBoneIndex.push_back(BoneIndex);
		BoneNameToBodyIndex[BoneName] = BodyIndex;
	}

	if (Bodies.empty())
	{
		UE_LOG("[RagdollBuildError] Owner=%s no runtime bodies were created from PhysicsAsset",
			MeshComp->GetName().c_str());
		Release(Scene);
		return false;
	}

	UE_LOG("[RagdollBuildSummary] Owner=%s RuntimeBodyCount=%d AssetBodyCount=%d",
		MeshComp->GetName().c_str(),
		static_cast<int32>(Bodies.size()),
		static_cast<int32>(BodySetups.size()));

	const TArray<FPhysicsConstraintSetup>& ConstraintSetups = PhysicsAsset->GetConstraintSetups();
	Constraints.reserve(ConstraintSetups.size());
	UE_LOG("[RagdollBuildConstraints] Owner=%s ConstraintCount=%d RuntimeBodyCount=%d",
		MeshComp->GetName().c_str(),
		static_cast<int32>(ConstraintSetups.size()),
		static_cast<int32>(Bodies.size()));

	for (const FPhysicsConstraintSetup& ConstraintSetup : ConstraintSetups)
	{
		const auto ParentBodyIt = BoneNameToBodyIndex.find(ConstraintSetup.ParentBoneName.ToString());
		const auto ChildBodyIt  = BoneNameToBodyIndex.find(ConstraintSetup.ChildBoneName.ToString());

		UE_LOG("[RagdollConstraintMap] Constraint=%s Parent=%s Child=%s FoundParent=%d ParentBody=%d FoundChild=%d ChildBody=%d",
			ConstraintSetup.ConstraintName.ToString().c_str(),
			ConstraintSetup.ParentBoneName.ToString().c_str(),
			ConstraintSetup.ChildBoneName.ToString().c_str(),
			ParentBodyIt != BoneNameToBodyIndex.end() ? 1 : 0,
			ParentBodyIt != BoneNameToBodyIndex.end() ? ParentBodyIt->second : -1,
			ChildBodyIt != BoneNameToBodyIndex.end() ? 1 : 0,
			ChildBodyIt != BoneNameToBodyIndex.end() ? ChildBodyIt->second : -1);

		if (ParentBodyIt == BoneNameToBodyIndex.end() || ChildBodyIt == BoneNameToBodyIndex.end())
		{
			continue;
		}

		FPhysicsConstraintDesc ConstraintDesc = ConstraintSetup.BuildConstraintDesc();
		ScaleConstraintDescForComponent(ConstraintDesc, StartScale);

		FPhysicsBodyInstance* ParentBody = Bodies[ParentBodyIt->second];
		FPhysicsBodyInstance* ChildBody = Bodies[ChildBodyIt->second];
		LogConstraintFrameError(ConstraintSetup, ConstraintDesc, ParentBody, ChildBody, Scene);

		FPhysicsConstraintInstance* ConstraintInstance = Scene->CreateConstraint(ParentBody, ChildBody, ConstraintDesc);
		if (ConstraintInstance)
		{
			Constraints.push_back(ConstraintInstance);
		}
		else
		{
			UE_LOG("[RagdollCreateConstraintError] Constraint=%s Parent=%s Child=%s failed to create runtime constraint",
				ConstraintSetup.ConstraintName.ToString().c_str(),
				ConstraintSetup.ParentBoneName.ToString().c_str(),
				ConstraintSetup.ChildBoneName.ToString().c_str());
		}
	}

	UE_LOG("[RagdollBuildConstraintsSummary] Owner=%s CreatedConstraintCount=%d AssetConstraintCount=%d",
		MeshComp->GetName().c_str(),
		static_cast<int32>(Constraints.size()),
		static_cast<int32>(ConstraintSetups.size()));

	Scene->RegisterRagdollInstanceStats(
		AggregateHandle != nullptr,
		static_cast<int32>(Bodies.size()),
		static_cast<int32>(Constraints.size()));
	bInitialized = true;
	return true;
}

static int32 FindRagdollFollowBodyIndex(const TArray<int32>& BodyToBoneIndex, const FSkeletonAsset* SkeletonAsset)
{
	if (!SkeletonAsset || BodyToBoneIndex.empty())
	{
		return INDEX_NONE;
	}

	for (int32 BodyIndex = 0; BodyIndex < static_cast<int32>(BodyToBoneIndex.size()); ++BodyIndex)
	{
		const int32 BoneIndex = BodyToBoneIndex[BodyIndex];
		if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(SkeletonAsset->Bones.size()))
		{
			continue;
		}
			
		const int32 ParentBoneIndex = SkeletonAsset->Bones[BoneIndex].ParentIndex;
		bool bParentHasRuntimeBody = false;
		for (const int32 CandidateBoneIndex : BodyToBoneIndex)
		{
			if (CandidateBoneIndex == ParentBoneIndex)
			{
				bParentHasRuntimeBody = true;
				break;
			}
		}

		if (!bParentHasRuntimeBody)
		{
			return BodyIndex;
		}
	}

	return 0;
}

static bool MakeComponentWorldFromFollow(
	const TArray<FMatrix>& InitialComponentPose,
	const FVector& StartScale,
	int32 BoneIndex,
	const FTransform& BodyWorld,
	FTransform& OutWorld)
{
	if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(InitialComponentPose.size()))
	{
		return false;
	}

	FTransform BoneInComp(InitialComponentPose[BoneIndex]);
	BoneInComp.Location = MultiplyVectorComponents(BoneInComp.Location, StartScale);
	BoneInComp.Scale = FVector::OneVector;

	const FTransform BodyNoScale(
		BodyWorld.Location,
		BodyWorld.Rotation.GetNormalized(),
		FVector::OneVector);

	OutWorld = FTransform(BoneInComp.ToMatrix().GetInverse() * BodyNoScale.ToMatrix());
	OutWorld.Scale = FVector::OneVector;
	return true;
}

static void ApplyWorldTransformToComponent(USkeletalMeshComponent* MeshComp, const FTransform& WorldTransform)
{
	if (!MeshComp)
	{
		return;
	}

	MeshComp->SetWorldLocation(WorldTransform.Location);

	const FQuat WorldRotation = WorldTransform.Rotation.GetNormalized();
	if (USceneComponent* Parent = MeshComp->GetParent())
	{
		const FQuat ParentWorldRotation = FTransform(Parent->GetWorldMatrix()).Rotation.GetNormalized();
		MeshComp->SetRelativeRotation((WorldRotation * ParentWorldRotation.Inverse()).GetNormalized());
		return;
	}

	MeshComp->SetRelativeRotation(WorldRotation);
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
		if (bInitialized)
		{
			Scene->UnregisterRagdollInstanceStats(
				AggregateHandle != nullptr,
				static_cast<int32>(Bodies.size()),
				static_cast<int32>(Constraints.size()));
		}
		for (FPhysicsConstraintInstance* ConstraintInstance : Constraints)
		{
			Scene->DestroyConstraint(ConstraintInstance);
		}
		for (FPhysicsBodyInstance* BodyInstance : Bodies)
		{
			Scene->DestroyBody(BodyInstance);
		}
		if (AggregateHandle)
		{
			Scene->DestroyAggregateHandle(AggregateHandle);
			AggregateHandle = nullptr;
		}
	}
	else
	{
		AggregateHandle = nullptr;
	}

	Constraints.clear();
	Bodies.clear();
	BodyToBoneIndex.clear();
	InitialLocalPose.clear();
	InitialComponentPose.clear();
	StartScale = FVector::OneVector;
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

	const int32 FollowBodyIndex = FindRagdollFollowBodyIndex(BodyToBoneIndex, SkeletonAsset);
	if (FollowBodyIndex >= 0 && FollowBodyIndex < static_cast<int32>(Bodies.size()))
	{
		FTransform FollowBodyWorldTransform;
		if (Scene->GetBodyWorldTransform(Bodies[FollowBodyIndex], FollowBodyWorldTransform))
		{
			FTransform CompWorld;
			const int32 BoneIndex = FollowBodyIndex < static_cast<int32>(BodyToBoneIndex.size())
				? BodyToBoneIndex[FollowBodyIndex]
				: INDEX_NONE;
			if (MakeComponentWorldFromFollow(InitialComponentPose, StartScale, BoneIndex, FollowBodyWorldTransform, CompWorld))
			{
				ApplyWorldTransformToComponent(MeshComp, CompWorld);
			}
		}
	}

	TArray<FMatrix> ComponentGlobals;
	ComponentGlobals.resize(BoneCount, FMatrix::Identity);
	TArray<bool> bSolved;
	bSolved.resize(BoneCount, false);

	const FMatrix ComponentWorld = MeshComp->GetWorldMatrix();
	const FMatrix ComponentWorldInv = ComponentWorld.GetInverse();
	const FQuat ComponentWorldRotation = FTransform(ComponentWorld).Rotation.GetNormalized();
	const FTransform ComponentWorldUnitScale(MeshComp->GetWorldLocation(), ComponentWorldRotation, FVector::OneVector);
	const FMatrix ComponentWorldUnitScaleInv = ComponentWorldUnitScale.ToMatrix().GetInverse();

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

		FMatrix ComponentGlobal = FTransform(BodyWorldTransform.Location, BodyWorldTransform.Rotation, FVector::OneVector).ToMatrix() * ComponentWorldUnitScaleInv;
		
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

