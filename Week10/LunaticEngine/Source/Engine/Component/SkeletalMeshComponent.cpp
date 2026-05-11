#include "Component/SkeletalMeshComponent.h"

#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Mesh/SkeletalMeshManager.h"
#include "Object/ObjectFactory.h"
#include "Render/Proxy/DirtyFlag.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"
#include "Serialization/Archive.h"

#include <cmath>
#include <cstring>
#include <string>

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

namespace
{
	int32 GetRequiredMaterialSlotCount(const USkeletalMesh* SkeletalMesh);
	bool IsNonePath(const FString& Path);
	void NormalizeAssetPath(FString& Path);
	void SerializeTransforms(FArchive& Ar, TArray<FTransform>& Transforms);
}

FMeshBuffer* USkeletalMeshComponent::GetMeshBuffer() const
{
	return SkeletalMeshAsset ? SkeletalMeshAsset->GetMeshBuffer() : nullptr;
}

FMeshDataView USkeletalMeshComponent::GetMeshDataView() const
{
	if (!SkeletalMeshAsset)
	{
		return {};
	}

	const FSkeletalMesh* MeshAsset = SkeletalMeshAsset->GetSkeletalMeshAsset();
	const FSkeletalMeshLOD* LOD = MeshAsset ? MeshAsset->GetLOD(0) : nullptr;
	if (!LOD || LOD->Vertices.empty())
	{
		return {};
	}

	FMeshDataView View;
	View.VertexData = LOD->Vertices.data();
	View.VertexCount = static_cast<uint32>(LOD->Vertices.size());
	View.Stride = sizeof(FSkeletalVertex);
	View.IndexData = LOD->Indices.data();
	View.IndexCount = static_cast<uint32>(LOD->Indices.size());
	return View;
}

void USkeletalMeshComponent::UpdateWorldAABB() const
{
	if (!bHasValidBounds)
	{
		UPrimitiveComponent::UpdateWorldAABB();
		return;
	}

	FVector WorldCenter = CachedWorldMatrix.TransformPositionWithW(CachedLocalCenter);

	float Ex = std::abs(CachedWorldMatrix.M[0][0]) * CachedLocalExtent.X
		+ std::abs(CachedWorldMatrix.M[1][0]) * CachedLocalExtent.Y
		+ std::abs(CachedWorldMatrix.M[2][0]) * CachedLocalExtent.Z;
	float Ey = std::abs(CachedWorldMatrix.M[0][1]) * CachedLocalExtent.X
		+ std::abs(CachedWorldMatrix.M[1][1]) * CachedLocalExtent.Y
		+ std::abs(CachedWorldMatrix.M[2][1]) * CachedLocalExtent.Z;
	float Ez = std::abs(CachedWorldMatrix.M[0][2]) * CachedLocalExtent.X
		+ std::abs(CachedWorldMatrix.M[1][2]) * CachedLocalExtent.Y
		+ std::abs(CachedWorldMatrix.M[2][2]) * CachedLocalExtent.Z;

	WorldAABBMinLocation = WorldCenter - FVector(Ex, Ey, Ez);
	WorldAABBMaxLocation = WorldCenter + FVector(Ex, Ey, Ez);
	bWorldAABBDirty = false;
	bHasValidWorldAABB = true;
}

FPrimitiveSceneProxy* USkeletalMeshComponent::CreateSceneProxy()
{
	return new FSkeletalMeshSceneProxy(this);
}

const FTransform* USkeletalMeshComponent::GetBoneLocalTransform(int32 BoneIndex) const
{
	return (BoneIndex >= 0 && BoneIndex < static_cast<int32>(BoneSpaceTransforms.size()))
		? &BoneSpaceTransforms[BoneIndex]
		: nullptr;
}

const FTransform* USkeletalMeshComponent::GetBoneComponentSpaceTransform(int32 BoneIndex) const
{
	return (BoneIndex >= 0 && BoneIndex < static_cast<int32>(ComponentSpaceTransforms.size()))
		? &ComponentSpaceTransforms[BoneIndex]
		: nullptr;
}

void USkeletalMeshComponent::SyncSkinnedAssetPathFromSkeletalMesh()
{
	NormalizeAssetPath(SkeletalMeshAssetPath);
	SkinnedMeshAssetPath = SkeletalMeshAssetPath;
	SkinnedAsset = SkeletalMeshAsset;
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

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InSkeletalMesh)
{
	SkeletalMeshAsset = InSkeletalMesh;
	SkeletalMeshAssetPath = SkeletalMeshAsset ? SkeletalMeshAsset->GetAssetPathFileName() : "None";
	SyncSkinnedAssetPathFromSkeletalMesh();
	EnsureMaterialSlotsForEditing();
	CacheLocalBounds();
	MarkRenderStateDirty();
	MarkSkeletalPoseDirty();
}

void USkeletalMeshComponent::EnsureMaterialSlotsForEditing()
{
	NormalizeAssetPath(SkeletalMeshAssetPath);

	const int32 RequiredSlotCount = GetRequiredMaterialSlotCount(SkeletalMeshAsset);
	if (RequiredSlotCount <= 0)
	{
		OverrideMaterials.clear();
		MaterialSlots.clear();
		return;
	}

	const int32 PreviousOverrideCount = static_cast<int32>(OverrideMaterials.size());
	const int32 PreviousSlotCount = static_cast<int32>(MaterialSlots.size());
	if (PreviousOverrideCount >= RequiredSlotCount && PreviousSlotCount >= RequiredSlotCount)
	{
		return;
	}

	const TArray<FStaticMaterial>& DefaultMaterials = SkeletalMeshAsset
		? SkeletalMeshAsset->GetSkeletalMaterials()
		: TArray<FStaticMaterial>{};

	OverrideMaterials.resize(RequiredSlotCount, nullptr);
	MaterialSlots.resize(RequiredSlotCount);

	for (int32 SlotIndex = 0; SlotIndex < RequiredSlotCount; ++SlotIndex)
	{
		if (SlotIndex >= PreviousOverrideCount)
		{
			if (SlotIndex < static_cast<int32>(DefaultMaterials.size()))
			{
				OverrideMaterials[SlotIndex] = DefaultMaterials[SlotIndex].MaterialInterface;
			}
			else
			{
				OverrideMaterials[SlotIndex] = FMaterialManager::Get().GetOrCreateMaterial("None");
			}
		}

		if (SlotIndex >= PreviousSlotCount || MaterialSlots[SlotIndex].Path.empty())
		{
			MaterialSlots[SlotIndex].Path = OverrideMaterials[SlotIndex]
				? OverrideMaterials[SlotIndex]->GetAssetPathFileName()
				: "None";
		}
	}
}

void USkeletalMeshComponent::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving())
	{
		SyncSkinnedAssetPathFromSkeletalMesh();
	}

	USkinnedMeshComponent::Serialize(Ar);

	Ar << SkeletalMeshAssetPath;
	SerializeTransforms(Ar, BoneSpaceTransforms);
	Ar << bForceRefPose;
	Ar << bEnableSkeletonUpdate;
	Ar << RootBoneTranslation;
	Ar << bShowSkeleton;
	Ar << bShowBoneNames;

	if (Ar.IsLoading())
	{
		NormalizeAssetPath(SkeletalMeshAssetPath);

		if (IsNonePath(SkeletalMeshAssetPath) && !IsNonePath(SkinnedMeshAssetPath))
		{
			SkeletalMeshAssetPath = SkinnedMeshAssetPath;
		}

		SyncSkinnedAssetPathFromSkeletalMesh();
		CacheLocalBounds();
		MarkSkeletalPoseDirty();
	}
}

void USkeletalMeshComponent::PostDuplicate()
{
	USkinnedMeshComponent::PostDuplicate();

	NormalizeAssetPath(SkeletalMeshAssetPath);
	if (IsNonePath(SkeletalMeshAssetPath) && !IsNonePath(SkinnedMeshAssetPath))
	{
		SkeletalMeshAssetPath = SkinnedMeshAssetPath;
	}

	SyncSkinnedAssetPathFromSkeletalMesh();
	CacheLocalBounds();
	MarkSkeletalPoseDirty();
}

void USkeletalMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	EnsureMaterialSlotsForEditing();

	UMeshComponent::GetEditableProperties(OutProps);

	OutProps.push_back({ "Skeletal Mesh", EPropertyType::SkeletalMeshRef, &SkeletalMeshAssetPath });
	OutProps.push_back({ "CPU Skinning", EPropertyType::Bool, &bCPUSkinning });
	OutProps.push_back({ "Display Bones", EPropertyType::Bool, &bDisplayBones });
	OutProps.push_back({ "Hide Skin", EPropertyType::Bool, &bHideSkin });
	OutProps.push_back({ "Force Ref Pose", EPropertyType::Bool, &bForceRefPose });
	OutProps.push_back({ "Enable Skeleton Update", EPropertyType::Bool, &bEnableSkeletonUpdate });
	OutProps.push_back({ "Root Bone", EPropertyType::Vec3, &RootBoneTranslation });
	OutProps.push_back({ "Selected Bone Index", EPropertyType::Int, &SelectedBoneIndex, -1.0f, 100000.0f, 1.0f });
	OutProps.push_back({ "Show Skeleton", EPropertyType::Bool, &bShowSkeleton });
	OutProps.push_back({ "Show Bone Names", EPropertyType::Bool, &bShowBoneNames });

	for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(MaterialSlots.size()); ++SlotIndex)
	{
		FPropertyDescriptor Desc;
		Desc.Name = "Element " + std::to_string(SlotIndex);
		Desc.Type = EPropertyType::MaterialSlot;
		Desc.ValuePtr = &MaterialSlots[SlotIndex];
		OutProps.push_back(Desc);
	}
}

void USkeletalMeshComponent::PostEditProperty(const char* PropertyName)
{
	USkinnedMeshComponent::PostEditProperty(PropertyName);

	if (std::strcmp(PropertyName, "Skeletal Mesh") == 0)
	{
		NormalizeAssetPath(SkeletalMeshAssetPath);

		if (IsNonePath(SkeletalMeshAssetPath))
		{
			SkeletalMeshAsset = nullptr;
			SkinnedAsset = nullptr;
		}
		else
		{
			SetSkeletalMesh(FSkeletalMeshManager::LoadSkeletalMesh(SkeletalMeshAssetPath));
		}

		SyncSkinnedAssetPathFromSkeletalMesh();
		EnsureMaterialSlotsForEditing();
		CacheLocalBounds();
		MarkSkeletalPoseDirty();
		return;
	}

	if (std::strcmp(PropertyName, "Force Ref Pose") == 0
		|| std::strcmp(PropertyName, "Enable Skeleton Update") == 0
		|| std::strcmp(PropertyName, "Root Bone Translation") == 0)
	{
		MarkSkeletalPoseDirty();
		return;
	}

	if (std::strcmp(PropertyName, "Selected Bone Index") == 0
		|| std::strcmp(PropertyName, "Show Skeleton") == 0
		|| std::strcmp(PropertyName, "Show Bone Names") == 0)
	{
		MarkProxyDirty(EDirtyFlag::Mesh);
	}
}

void USkeletalMeshComponent::CacheLocalBounds()
{
	bHasValidBounds = false;
	if (!SkeletalMeshAsset)
	{
		CachedLocalCenter = FVector(0.0f, 0.0f, 0.0f);
		CachedLocalExtent = FVector(0.5f, 0.5f, 0.5f);
		return;
	}

	FSkeletalMesh* MeshAsset = SkeletalMeshAsset->GetSkeletalMeshAsset();
	if (!MeshAsset)
	{
		CachedLocalCenter = FVector(0.0f, 0.0f, 0.0f);
		CachedLocalExtent = FVector(0.5f, 0.5f, 0.5f);
		return;
	}

	FSkeletalMeshLOD* LOD = MeshAsset->GetLOD(0);
	if (!LOD)
	{
		CachedLocalCenter = FVector(0.0f, 0.0f, 0.0f);
		CachedLocalExtent = FVector(0.5f, 0.5f, 0.5f);
		return;
	}

	if (!LOD->bBoundsValid)
	{
		LOD->CacheBounds();
	}

	CachedLocalCenter = LOD->BoundsCenter;
	CachedLocalExtent = LOD->BoundsExtent;
	bHasValidBounds = LOD->bBoundsValid;
}

namespace
{
	int32 GetRequiredMaterialSlotCount(const USkeletalMesh* SkeletalMesh)
	{
		if (!SkeletalMesh)
		{
			return 0;
		}

		const TArray<FStaticMaterial>& Materials = SkeletalMesh->GetSkeletalMaterials();
		if (!Materials.empty())
		{
			return static_cast<int32>(Materials.size());
		}

		const FSkeletalMesh* MeshAsset = SkeletalMesh->GetSkeletalMeshAsset();
		const FSkeletalMeshLOD* LOD = MeshAsset ? MeshAsset->GetLOD(0) : nullptr;
		if (LOD && (!LOD->Sections.empty() || !LOD->Indices.empty()))
		{
			return 1;
		}

		return 0;
	}

	bool IsNonePath(const FString& Path)
	{
		return Path.empty() || Path == "None";
	}

	void NormalizeAssetPath(FString& Path)
	{
		if (Path.empty())
		{
			Path = "None";
		}
	}

	void SerializeTransforms(FArchive& Ar, TArray<FTransform>& Transforms)
	{
		uint32 TransformCount = static_cast<uint32>(Transforms.size());
		Ar << TransformCount;

		if (Ar.IsLoading())
		{
			Transforms.resize(TransformCount);
		}

		for (FTransform& Transform : Transforms)
		{
			Ar << Transform.Location;
			Ar << Transform.Rotation;
			Ar << Transform.Scale;
		}
	}
}
