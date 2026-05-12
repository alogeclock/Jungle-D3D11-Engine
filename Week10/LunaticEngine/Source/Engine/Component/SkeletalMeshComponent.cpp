#include "Component/SkeletalMeshComponent.h"

#include "Collision/RayUtils.h"
#include "Materials/MaterialManager.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Mesh/SkeletalMeshManager.h"
#include "Object/ObjectFactory.h"
#include "Render/Proxy/DirtyFlag.h"
#include "Render/Scene/FScene.h"
#include "Serialization/Archive.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include <cfloat>
#include <cmath>

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

namespace
{
	bool IsNonePath(const FString& Path) { return Path.empty() || Path == "None"; }

	constexpr int32 BoneArmatureRingSegments = 12;
	constexpr float BoneArmatureMinLength = 0.001f;
	constexpr float BoneArmatureRadiusRatio = 0.08f;
	constexpr float BoneArmatureMinRadius = 0.001f;
	constexpr float BoneArmatureMaxRadius = 5.0f;
	constexpr float BoneArmatureTipBias = 0.05f;

	struct FBoneArmatureFrame
	{
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		FVector Axis = FVector(1.0f, 0.0f, 0.0f);
		FVector Right = FVector(0.0f, 1.0f, 0.0f);
		FVector Up = FVector(0.0f, 0.0f, 1.0f);
		float Length = 0.0f;
		float Radius = 1.0f;
	};

	FTransform TransformFromMatrix(const FMatrix& Matrix)
	{
		FQuat Rotation = Matrix.ToQuat();
		Rotation.Normalize();
		return FTransform(Matrix.GetLocation(), Rotation, Matrix.GetScale());
	}

	float ClampFloat(float Value, float MinValue, float MaxValue)
	{
		return (std::max)(MinValue, (std::min)(Value, MaxValue));
	}

	bool MakeBoneArmatureFrame(const FVector& Start, const FVector& End, FBoneArmatureFrame& OutFrame)
	{
		const FVector Delta = End - Start;
		const float Length = Delta.Length();
		if (Length <= BoneArmatureMinLength)
		{
			return false;
		}

		OutFrame.Start = Start;
		OutFrame.End = End;
		OutFrame.Axis = Delta / Length;
		OutFrame.Length = Length;
		OutFrame.Radius = ClampFloat(Length * BoneArmatureRadiusRatio, BoneArmatureMinRadius, BoneArmatureMaxRadius);

		const FVector UpHint = std::abs(OutFrame.Axis.Z) < 0.92f ? FVector(0.0f, 0.0f, 1.0f) : FVector(0.0f, 1.0f, 0.0f);
		OutFrame.Right = UpHint.Cross(OutFrame.Axis).Normalized();
		OutFrame.Up = OutFrame.Axis.Cross(OutFrame.Right).Normalized();
		return true;
	}

	FVector GetArmatureWidePoint(const FBoneArmatureFrame& Frame)
	{
		return Frame.Start + Frame.Axis * (Frame.Length * BoneArmatureTipBias);
	}

	void AddArmatureLine(FScene& Scene, const FVector& Start, const FVector& End, const FColor& Color)
	{
		Scene.AddForegroundDebugLine(Start, End, Color);
	}

	void AddArmatureTipRings(FScene& Scene, const FBoneArmatureFrame& Frame, const FColor& Color)
	{
		const float TipRadius = Frame.Radius * 0.2f;
		for (int32 Segment = 0; Segment < BoneArmatureRingSegments; ++Segment)
		{
			const float A0 = (static_cast<float>(Segment) / static_cast<float>(BoneArmatureRingSegments)) * 6.28318530718f;
			const float A1 = (static_cast<float>(Segment + 1) / static_cast<float>(BoneArmatureRingSegments)) * 6.28318530718f;
			const float C0 = std::cos(A0);
			const float S0 = std::sin(A0);
			const float C1 = std::cos(A1);
			const float S1 = std::sin(A1);

			AddArmatureLine(Scene, Frame.End + Frame.Right * (C0 * TipRadius) + Frame.Up * (S0 * TipRadius),
				Frame.End + Frame.Right * (C1 * TipRadius) + Frame.Up * (S1 * TipRadius), Color);
			AddArmatureLine(Scene, Frame.End + Frame.Axis * (C0 * TipRadius) + Frame.Up * (S0 * TipRadius),
				Frame.End + Frame.Axis * (C1 * TipRadius) + Frame.Up * (S1 * TipRadius), Color);
			AddArmatureLine(Scene, Frame.End + Frame.Axis * (C0 * TipRadius) + Frame.Right * (S0 * TipRadius),
				Frame.End + Frame.Axis * (C1 * TipRadius) + Frame.Right * (S1 * TipRadius), Color);
		}
	}

	void AddArmatureBody(FScene& Scene, const FBoneArmatureFrame& Frame, const FColor& Color)
	{
		const FVector WidePoint = GetArmatureWidePoint(Frame);
		const FVector Equator[4] =
		{
			WidePoint + Frame.Right * Frame.Radius,
			WidePoint + Frame.Up * Frame.Radius,
			WidePoint - Frame.Right * Frame.Radius,
			WidePoint - Frame.Up * Frame.Radius
		};

		for (int32 Index = 0; Index < 4; ++Index)
		{
			const FVector& Current = Equator[Index];
			const FVector& Next = Equator[(Index + 1) % 4];
			AddArmatureLine(Scene, Frame.Start, Current, Color);
			AddArmatureLine(Scene, Frame.End, Current, Color);
			AddArmatureLine(Scene, Current, Next, Color);
		}
	}

	void AddArmatureBodyTriangles(const FBoneArmatureFrame& Frame, TArray<FVector>& OutVertices)
	{
		const FVector WidePoint = GetArmatureWidePoint(Frame);
		const FVector Equator[4] =
		{
			WidePoint + Frame.Right * Frame.Radius,
			WidePoint + Frame.Up * Frame.Radius,
			WidePoint - Frame.Right * Frame.Radius,
			WidePoint - Frame.Up * Frame.Radius
		};

		for (int32 Index = 0; Index < 4; ++Index)
		{
			const FVector& Current = Equator[Index];
			const FVector& Next = Equator[(Index + 1) % 4];
			OutVertices.push_back(Frame.Start);
			OutVertices.push_back(Current);
			OutVertices.push_back(Next);
			OutVertices.push_back(Frame.End);
			OutVertices.push_back(Next);
			OutVertices.push_back(Current);
		}
	}

	bool RaycastArmatureFrame(const FRay& Ray, const FBoneArmatureFrame& Frame, float& OutDistance)
	{
		TArray<FVector> Triangles;
		Triangles.reserve(24);
		AddArmatureBodyTriangles(Frame, Triangles);

		bool bHit = false;
		float Closest = FLT_MAX;
		for (int32 Index = 0; Index + 2 < static_cast<int32>(Triangles.size()); Index += 3)
		{
			float T = 0.0f;
			if (FRayUtils::IntersectTriangle(Ray.Origin, Ray.Direction, Triangles[Index], Triangles[Index + 1], Triangles[Index + 2], T) && T < Closest)
			{
				Closest = T;
				bHit = true;
			}
		}

		OutDistance = Closest;
		return bHit;
	}
}

USkeletalMeshComponent::~USkeletalMeshComponent() = default;

FMeshDataView USkeletalMeshComponent::GetMeshDataView() const
{
	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const FSkeletalMeshLOD* LOD = MeshAsset ? MeshAsset->GetLOD(0) : nullptr;
	if (!LOD || LOD->Vertices.empty()) return {};

	FMeshDataView View;
	View.VertexData = LOD->Vertices.data();
	View.VertexCount = static_cast<uint32>(LOD->Vertices.size());
	View.Stride = sizeof(FSkeletalVertex);
	View.IndexData = LOD->Indices.data();
	View.IndexCount = static_cast<uint32>(LOD->Indices.size());
	return View;
}

void USkeletalMeshComponent::ContributeVisuals(FScene& Scene) const
{
	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!ShouldDisplayBones() || !MeshAsset || MeshAsset->Skeleton.Bones.empty()) return;

	const FSkeleton& Skeleton = MeshAsset->Skeleton;
	const FMatrix& ComponentWorld = GetWorldMatrix();

	auto GetBoneMatrix = [&](int32 BoneIndex) -> FMatrix
	{
		if (BoneIndex >= 0 && BoneIndex < static_cast<int32>(ComponentSpaceTransforms.size()))
			return ComponentSpaceTransforms[BoneIndex].ToMatrix();
		return Skeleton.Bones[BoneIndex].GlobalBindPose;
	};

	for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Skeleton.Bones.size()); ++BoneIndex)
	{
		const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;
		if (!IsValidBoneIndex(ParentIndex)) continue;

		const FVector ParentWorld = ComponentWorld.TransformPositionWithW(GetBoneMatrix(ParentIndex).GetLocation());
		const FVector BoneWorld = ComponentWorld.TransformPositionWithW(GetBoneMatrix(BoneIndex).GetLocation());
		const bool bSelected = BoneIndex == SelectedBoneIndex || ParentIndex == SelectedBoneIndex;
		FBoneArmatureFrame Frame;
		if (!MakeBoneArmatureFrame(ParentWorld, BoneWorld, Frame))
		{
			continue;
		}

		const FColor Color = bSelected ? FColor::Orange() : FColor(255, 255, 255, 96);
		AddArmatureBody(Scene, Frame, Color);
		AddArmatureTipRings(Scene, Frame, Color);
	}
}

const FTransform* USkeletalMeshComponent::GetBoneLocalTransform(int32 BoneIndex) const
{
	return IsValidBoneIndex(BoneIndex) ? &BoneSpaceTransforms[BoneIndex] : nullptr;
}

const FTransform* USkeletalMeshComponent::GetBoneComponentSpaceTransform(int32 BoneIndex) const
{
	return IsValidBoneIndex(BoneIndex) ? &ComponentSpaceTransforms[BoneIndex] : nullptr;
}

void USkeletalMeshComponent::SetSelectedBoneIndex(int32 BoneIndex)
{
	SelectedBoneIndex = IsValidBoneIndex(BoneIndex) ? BoneIndex : -1;
}

int32 USkeletalMeshComponent::PickBoneArmature(const FRay& Ray, float* OutDistance) const
{
	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!MeshAsset || MeshAsset->Skeleton.Bones.empty())
	{
		if (OutDistance) *OutDistance = FLT_MAX;
		return -1;
	}

	const FSkeleton& Skeleton = MeshAsset->Skeleton;
	const FMatrix& ComponentWorld = GetWorldMatrix();

	float BestDistance = FLT_MAX;
	int32 BestBoneIndex = -1;

	auto GetBoneMatrix = [&](int32 BoneIndex) -> FMatrix
	{
		if (BoneIndex >= 0 && BoneIndex < static_cast<int32>(ComponentSpaceTransforms.size()))
		{
			return ComponentSpaceTransforms[BoneIndex].ToMatrix();
		}
		return Skeleton.Bones[BoneIndex].GlobalBindPose;
	};

	for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Skeleton.Bones.size()); ++BoneIndex)
	{
		const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;
		if (!IsValidBoneIndex(ParentIndex))
		{
			continue;
		}

		const FVector ParentWorld = ComponentWorld.TransformPositionWithW(GetBoneMatrix(ParentIndex).GetLocation());
		const FVector BoneWorld = ComponentWorld.TransformPositionWithW(GetBoneMatrix(BoneIndex).GetLocation());

		FBoneArmatureFrame Frame;
		float HitDistance = FLT_MAX;
		if (MakeBoneArmatureFrame(ParentWorld, BoneWorld, Frame) &&
			RaycastArmatureFrame(Ray, Frame, HitDistance) &&
			HitDistance < BestDistance)
		{
			BestDistance = HitDistance;
			BestBoneIndex = BoneIndex;
		}
	}

	if (OutDistance)
	{
		*OutDistance = BestDistance;
	}
	return BestBoneIndex;
}

void USkeletalMeshComponent::SetBoneLocalTransform(int32 BoneIndex, const FTransform& NewTransform)
{
	if (!IsValidBoneIndex(BoneIndex)) return;

	if (BoneSpaceTransforms.size() != ComponentSpaceTransforms.size())
		InitializeBoneTransformsFromSkeleton();

	FTransform NormalizedTransform = NewTransform;
	NormalizedTransform.Rotation.Normalize();
	BoneSpaceTransforms[BoneIndex] = NormalizedTransform;
	
	MarkSkeletalPoseDirty();
	RefreshBoneTransforms();
	UpdateSkinnedMeshObject();
}

bool USkeletalMeshComponent::SetBoneComponentSpaceTransform(int32 BoneIndex, const FTransform& NewTransform)
{
	if (!IsValidBoneIndex(BoneIndex)) return false;

	const FSkeleton& Skeleton = GetSkeletalMesh()->GetSkeletalMeshAsset()->Skeleton;
	FMatrix LocalMatrix = NewTransform.ToMatrix();
	const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;
	if (IsValidBoneIndex(ParentIndex))
	{
		const FMatrix ParentMatrix = ParentIndex < static_cast<int32>(ComponentSpaceTransforms.size())
			? ComponentSpaceTransforms[ParentIndex].ToMatrix()
			: Skeleton.Bones[ParentIndex].GlobalBindPose;
		LocalMatrix = LocalMatrix * ParentMatrix.GetInverse();
	}

	SetBoneLocalTransform(BoneIndex, TransformFromMatrix(LocalMatrix));
	return true;
}

bool USkeletalMeshComponent::SetBoneComponentSpaceRotation(int32 BoneIndex, const FQuat& NewComponentRotation)
{
	if (!IsValidBoneIndex(BoneIndex)) return false;
	if (BoneSpaceTransforms.size() != ComponentSpaceTransforms.size()) InitializeBoneTransformsFromSkeleton();

	const FSkeleton& Skeleton = GetSkeletalMesh()->GetSkeletalMeshAsset()->Skeleton;
	FQuat NewRotation = NewComponentRotation.GetNormalized();
	const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;
	if (IsValidBoneIndex(ParentIndex))
	{
		FQuat ParentRot = ParentIndex < static_cast<int32>(ComponentSpaceTransforms.size())
			? ComponentSpaceTransforms[ParentIndex].Rotation
			: Skeleton.Bones[ParentIndex].GlobalBindPose.ToQuat();
		ParentRot.Normalize();
		NewRotation = (ParentRot.Inverse() * NewRotation).GetNormalized();
	}

	FTransform LocalTransform = BoneSpaceTransforms[BoneIndex];
	LocalTransform.Rotation = NewRotation;
	SetBoneLocalTransform(BoneIndex, LocalTransform);
	return true;
}

void USkeletalMeshComponent::RefreshBoneTransforms()
{
	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const FSkeleton* Skeleton = MeshAsset ? &MeshAsset->Skeleton : nullptr;
	const int32 BoneCount = Skeleton ? static_cast<int32>(Skeleton->Bones.size()) : 0;
	
	if (BoneCount == 0) return;
	if (BoneSpaceTransforms.size() != BoneCount) { InitializeBoneTransformsFromSkeleton(); return; }

	ComponentSpaceTransforms.resize(BoneCount);
	ComponentSpaceMatrices.assign(BoneCount, FMatrix::Identity);
	SkinningMatrices.assign(BoneCount, FMatrix::Identity);
	RequiredBones.resize(BoneCount);

	FVector MinBound(1e10f, 1e10f, 1e10f), MaxBound(-1e10f, -1e10f, -1e10f);

	for (int32 i = 0; i < BoneCount; ++i)
	{
		RequiredBones[i] = i;
		const int32 Parent = Skeleton->Bones[i].ParentIndex;
		const FTransform& Local = BoneSpaceTransforms[i];

		if (Parent >= 0 && Parent < i)
		{
			const FTransform& PTransform = ComponentSpaceTransforms[Parent];
			FTransform& OutT = ComponentSpaceTransforms[i];
			
			OutT.Rotation = (PTransform.Rotation * Local.Rotation).GetNormalized();
			OutT.Location = PTransform.Location + PTransform.Rotation.RotateVector(PTransform.Scale * Local.Location);
			OutT.Scale = PTransform.Scale * Local.Scale;

			ComponentSpaceMatrices[i] = Local.ToMatrix() * ComponentSpaceMatrices[Parent];
		}
		else
		{
			ComponentSpaceTransforms[i] = Local;
			ComponentSpaceMatrices[i] = Local.ToMatrix();
		}

		SkinningMatrices[i] = Skeleton->Bones[i].InverseBindPose * ComponentSpaceMatrices[i];

		// Dynamic Bounds Calculation
		FVector Loc = ComponentSpaceTransforms[i].Location;
		MinBound.X = std::min(MinBound.X, Loc.X); MinBound.Y = std::min(MinBound.Y, Loc.Y); MinBound.Z = std::min(MinBound.Z, Loc.Z);
		MaxBound.X = std::max(MaxBound.X, Loc.X); MaxBound.Y = std::max(MaxBound.Y, Loc.Y); MaxBound.Z = std::max(MaxBound.Z, Loc.Z);
	}

	// Apply Bounds Scale and Padding
	CachedLocalCenter = (MinBound + MaxBound) * 0.5f;
	CachedLocalExtent = (MaxBound - MinBound) * 0.5f * std::max(BoundsScale, 0.1f);
	CachedLocalExtent.X = std::max(CachedLocalExtent.X, 2.0f);
	CachedLocalExtent.Y = std::max(CachedLocalExtent.Y, 2.0f);
	CachedLocalExtent.Z = std::max(CachedLocalExtent.Z, 2.0f);
	bHasValidBounds = true;

	bRequiredBonesUpdated = true;
	bPoseDirty = false;
	bSkinningDirty = false;
	bBoundsDirty = true;
	MarkWorldBoundsDirty();
}

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
	SetSkeletalMeshInternal(InSkeletalMesh, false, false);
	InitializeBoneTransformsFromSkeleton();
	FinalizeSkeletalMeshRenderState();
}

void USkeletalMeshComponent::Serialize(FArchive& Ar)
{
	USkinnedMeshComponent::Serialize(Ar);
	Ar << BoneSpaceTransforms;
	Ar << bForceRefPose;
	Ar << bEnableSkeletonUpdate;
	Ar << RootBoneTranslation;
	Ar << bShowBoneNames;

	if (Ar.IsLoading())
	{
		if (BoneSpaceTransforms.empty()) InitializeBoneTransformsFromSkeleton();
		else RefreshBoneTransforms();
	}
}

void USkeletalMeshComponent::PostDuplicate()
{
	USkinnedMeshComponent::PostDuplicate();
	RefreshBoneTransforms();
}

void USkeletalMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	EnsureMaterialSlotsForEditing();
	UMeshComponent::GetEditableProperties(OutProps);

	OutProps.push_back({ "Skeletal Mesh", EPropertyType::SkeletalMeshRef, &SkeletalMeshPath });
	OutProps.push_back({ "CPU Skinning", EPropertyType::Bool, &bCPUSkinning });
	OutProps.push_back({ "Bounds Scale", EPropertyType::Float, &BoundsScale, 0.1f, 10.0f, 0.1f });
	OutProps.push_back({ "Display Bones", EPropertyType::Bool, &bDisplayBones });
	OutProps.push_back({ "Show Bone Names", EPropertyType::Bool, &bShowBoneNames });
	OutProps.push_back({ "Selected Bone Index", EPropertyType::Int, &SelectedBoneIndex, -1.0f, 100000.0f, 1.0f });

	for (int32 i = 0; i < (int32)MaterialSlots.size(); ++i)
		OutProps.push_back({ "Element " + std::to_string(i), EPropertyType::MaterialSlot, &MaterialSlots[i] });
}

void USkeletalMeshComponent::PostEditProperty(const char* PropertyName)
{
	UMeshComponent::PostEditProperty(PropertyName);

	if (std::strcmp(PropertyName, "Skeletal Mesh") == 0)
		SetSkeletalMesh(IsNonePath(SkeletalMeshPath) ? nullptr : FSkeletalMeshManager::LoadSkeletalMesh(SkeletalMeshPath));
	else if (std::strncmp(PropertyName, "Element ", 8) == 0)
	{
		const int32 idx = std::atoi(&PropertyName[8]);
		if (idx >= 0 && idx < (int32)MaterialSlots.size())
			SetMaterial(idx, IsNonePath(MaterialSlots[idx].Path) ? nullptr : FMaterialManager::Get().GetOrCreateMaterial(MaterialSlots[idx].Path));
	}
	else if (std::strcmp(PropertyName, "Bounds Scale") == 0 || std::strcmp(PropertyName, "Force Ref Pose") == 0)
		MarkSkeletalPoseDirty();
	else if (std::strcmp(PropertyName, "Selected Bone Index") == 0 || std::strcmp(PropertyName, "Display Bones") == 0)
		MarkProxyDirty(EDirtyFlag::Mesh);
}

void USkeletalMeshComponent::MarkSkeletalPoseDirty()
{
	bRequiredBonesUpdated = false;
	bPoseDirty = true;
	bSkinningDirty = true;
	bBoundsDirty = true;
	MarkProxyDirty(EDirtyFlag::Mesh);
	MarkWorldBoundsDirty();
}

void USkeletalMeshComponent::InitializeBoneTransformsFromSkeleton()
{
	BoneSpaceTransforms.clear();
	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	const FSkeleton* Skeleton = MeshAsset ? &MeshAsset->Skeleton : nullptr;
	
	if (!Skeleton || Skeleton->Bones.empty())
	{
		SelectedBoneIndex = -1;
		bRequiredBonesUpdated = true;
		bPoseDirty = bSkinningDirty = false;
		return;
	}

	for (const FBoneInfo& Bone : Skeleton->Bones)
		BoneSpaceTransforms.push_back(TransformFromMatrix(Bone.LocalBindPose));

	RefreshBoneTransforms();
	UpdateSkinnedMeshObject();
}

bool USkeletalMeshComponent::IsValidBoneIndex(int32 BoneIndex) const
{
	const USkeletalMesh* Mesh = GetSkeletalMesh();
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	return MeshAsset && BoneIndex >= 0 && BoneIndex < (int32)MeshAsset->Skeleton.Bones.size();
}
