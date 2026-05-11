#include "SkinnedMeshComponent.h"
#include "Component/SkeletalMeshComponent.h"

#include <algorithm>
#include <cmath>

#include "Object/ObjectFactory.h"
#include "Core/PropertyTypes.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Engine/Runtime/Engine.h"
#include "Materials/MaterialManager.h"
#include "Render/Proxy/SkeletalMeshSceneProxy.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(USkeletalMeshComponent, UMeshComponent)

namespace
{
	int32 GetRequiredMaterialSlotCount(const USkeletalMesh* SkeletalMesh)
	{
		if (!SkeletalMesh)
		{
			return 0;
		}

		const TArray<FStaticMaterial>& DefaultMaterials = SkeletalMesh->GetSkeletalMaterials();
		if (!DefaultMaterials.empty())
		{
			return static_cast<int32>(DefaultMaterials.size());
		}

		const FSkeletalMesh* MeshAsset = SkeletalMesh->GetSkeletalMeshAsset();
		if (MeshAsset && !MeshAsset->LODModels.empty())
		{
			const FSkeletalMeshLOD& LOD0 = MeshAsset->LODModels[0];
			if (!LOD0.Sections.empty() || !LOD0.Indices.empty())
			{
				return 1;
			}
		}

		return 0;
	}

	void EnsureMaterialSlotStorage(USkeletalMesh* SkeletalMesh, TArray<UMaterial*>& OverrideMaterials, TArray<FMaterialSlot>& MaterialSlots)
	{
		const int32 RequiredSlotCount = GetRequiredMaterialSlotCount(SkeletalMesh);
		if (RequiredSlotCount <= 0)
		{
			return;
		}

		const int32 PreviousOverrideCount = static_cast<int32>(OverrideMaterials.size());
		const int32 PreviousSlotCount = static_cast<int32>(MaterialSlots.size());
		if (PreviousOverrideCount >= RequiredSlotCount && PreviousSlotCount >= RequiredSlotCount)
		{
			return;
		}

		const TArray<FStaticMaterial>& DefaultMaterials = SkeletalMesh ? SkeletalMesh->GetSkeletalMaterials() : TArray<FStaticMaterial>{};

		OverrideMaterials.resize(RequiredSlotCount, nullptr);
		MaterialSlots.resize(RequiredSlotCount);

		for (int32 i = 0; i < RequiredSlotCount; ++i)
		{
			if (i >= PreviousOverrideCount)
			{
				if (i < static_cast<int32>(DefaultMaterials.size()))
				{
					OverrideMaterials[i] = DefaultMaterials[i].MaterialInterface;
				}
				else
				{
					OverrideMaterials[i] = FMaterialManager::Get().GetOrCreateMaterial("None");
				}
			}

			if (i >= PreviousSlotCount || MaterialSlots[i].Path.empty())
			{
				MaterialSlots[i].Path = OverrideMaterials[i]
					? OverrideMaterials[i]->GetAssetPathFileName()
					: "None";
			}
		}
	}
}

FPrimitiveSceneProxy* USkeletalMeshComponent::CreateSceneProxy()
{
	return new FSkeletalMeshSceneProxy(this);
}

void USkeletalMeshComponent::SetSkeletalMesh(USkeletalMesh* InMesh)
{
	SkeletalMesh = InMesh;
	if (InMesh)
	{
		SkeletalMeshPath = InMesh->GetAssetPathFileName();
		OverrideMaterials.clear();
		MaterialSlots.clear();
		EnsureMaterialSlotStorage(SkeletalMesh, OverrideMaterials, MaterialSlots);
	}
	else
	{
		SkeletalMeshPath = "None";
		OverrideMaterials.clear();
		MaterialSlots.clear();
	}
	CacheLocalBounds();
	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
}

void USkeletalMeshComponent::CacheLocalBounds()
{
	bHasValidBounds = false;
	if (!SkeletalMesh) return;

	FSkeletalMesh* Asset = SkeletalMesh->GetSkeletalMeshAsset();
	if (!Asset || Asset->LODModels.empty()) return;

	FSkeletalMeshLOD& LOD0 = Asset->LODModels[0];
	if (!LOD0.bBoundsValid)
	{
		LOD0.CacheBounds();
	}

	CachedLocalCenter = LOD0.BoundsCenter;
	CachedLocalExtent = LOD0.BoundsExtent;
	bHasValidBounds = LOD0.bBoundsValid;
}

USkeletalMesh* USkeletalMeshComponent::GetSkeletalMesh() const
{
	return SkeletalMesh;
}

void USkeletalMeshComponent::EnsureMaterialSlotsForEditing()
{
	EnsureMaterialSlotStorage(SkeletalMesh, OverrideMaterials, MaterialSlots);
}

void USkeletalMeshComponent::SetMaterial(int32 ElementIndex, UMaterial* InMaterial)
{
	if (ElementIndex < 0)
	{
		return;
	}

	const int32 RequiredSlotCount = GetRequiredMaterialSlotCount(SkeletalMesh);
	if (ElementIndex >= static_cast<int32>(OverrideMaterials.size()) && ElementIndex < RequiredSlotCount)
	{
		const int32 NewSlotCount = std::max(RequiredSlotCount, ElementIndex + 1);
		OverrideMaterials.resize(NewSlotCount, nullptr);
		MaterialSlots.resize(NewSlotCount);

		for (int32 SlotIndex = 0; SlotIndex < NewSlotCount; ++SlotIndex)
		{
			if (MaterialSlots[SlotIndex].Path.empty())
			{
				MaterialSlots[SlotIndex].Path = "None";
			}
		}
	}

	if (ElementIndex < static_cast<int32>(OverrideMaterials.size()))
	{
		OverrideMaterials[ElementIndex] = InMaterial;

		if (ElementIndex < static_cast<int32>(MaterialSlots.size()))
		{
			MaterialSlots[ElementIndex].Path = InMaterial
				? InMaterial->GetAssetPathFileName()
				: "None";
		}

		MarkProxyDirty(EDirtyFlag::Material);
	}
}

UMaterial* USkeletalMeshComponent::GetMaterial(int32 ElementIndex) const
{
	if (ElementIndex >= 0 && ElementIndex < OverrideMaterials.size())
	{
		return OverrideMaterials[ElementIndex];
	}
	return nullptr;
}

FMaterialSlot* USkeletalMeshComponent::GetMaterialSlot(int32 ElementIndex)
{
	EnsureMaterialSlotsForEditing();
	return (ElementIndex >= 0 && ElementIndex < static_cast<int32>(MaterialSlots.size()))
		? &MaterialSlots[ElementIndex]
		: nullptr;
}

const FMaterialSlot* USkeletalMeshComponent::GetMaterialSlot(int32 ElementIndex) const
{
	return (ElementIndex >= 0 && ElementIndex < static_cast<int32>(MaterialSlots.size()))
		? &MaterialSlots[ElementIndex]
		: nullptr;
}

// FSkeletalMeshLOD에는 RenderBuffer가 없음 (GPU 업로드 단계 미구현).
// 일단 nullptr — 렌더 패스에서는 이 컴포넌트가 그려지지 않음.
FMeshBuffer* USkeletalMeshComponent::GetMeshBuffer() const
{
	return nullptr;
}

// FSkeletalVertex 레이아웃이 FNormalVertex와 달라 직접 노출할 수 없음.
FMeshDataView USkeletalMeshComponent::GetMeshDataView() const
{
	return {};
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

// 스켈레탈 메시는 매 프레임 변형되므로 bind-pose BVH는 정확하지 않음.
// 현재는 picking 미지원.
bool USkeletalMeshComponent::LineTraceComponent(const FRay& /*Ray*/, FRayHitResult& /*OutHitResult*/)
{
	return false;
}

static FArchive& operator<<(FArchive& Ar, FMaterialSlot& Slot)
{
	Ar << Slot.Path;
	return Ar;
}

void USkeletalMeshComponent::Serialize(FArchive& Ar)
{
	UMeshComponent::Serialize(Ar);
	Ar << SkeletalMeshPath;
	Ar << MaterialSlots;
}

void USkeletalMeshComponent::PostDuplicate()
{
	UMeshComponent::PostDuplicate();

	// USkeletalMesh 전용 로더는 후속 단계에서 연결.
	// SkeletalMeshManager 등을 통해 SkeletalMeshPath로 다시 로드하는 흐름이 들어갈 자리.

	CacheLocalBounds();
	MarkRenderStateDirty();
	MarkWorldBoundsDirty();
}

void USkeletalMeshComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	EnsureMaterialSlotsForEditing();

	UPrimitiveComponent::GetEditableProperties(OutProps);
	OutProps.push_back({ "Skeletal Mesh", EPropertyType::StaticMeshRef, &SkeletalMeshPath });

	for (int32 i = 0; i < (int32)MaterialSlots.size(); ++i)
	{
		FPropertyDescriptor Desc;
		Desc.Name = "Element " + std::to_string(i);
		Desc.Type = EPropertyType::MaterialSlot;
		Desc.ValuePtr = &MaterialSlots[i];
		OutProps.push_back(Desc);
	}
}

void USkeletalMeshComponent::PostEditProperty(const char* PropertyName)
{
	UPrimitiveComponent::PostEditProperty(PropertyName);

	if (strcmp(PropertyName, "Skeletal Mesh") == 0)
	{
		if (SkeletalMeshPath.empty() || SkeletalMeshPath == "None")
		{
			SkeletalMesh = nullptr;
		}
		// USkeletalMesh 로더 연결은 후속 단계.
		CacheLocalBounds();
		MarkWorldBoundsDirty();
	}

	if (strncmp(PropertyName, "Element ", 8) == 0)
	{
		int32 Index = atoi(&PropertyName[8]);

		if (Index >= 0 && Index < (int32)MaterialSlots.size())
		{
			FString NewMatPath = MaterialSlots[Index].Path;

			if (NewMatPath == "None" || NewMatPath.empty())
			{
				SetMaterial(Index, nullptr);
			}
			else
			{
				UMaterial* LoadedMat = FMaterialManager::Get().GetOrCreateMaterial(NewMatPath);
				if (LoadedMat)
				{
					SetMaterial(Index, LoadedMat);
				}
			}
		}
	}
}
