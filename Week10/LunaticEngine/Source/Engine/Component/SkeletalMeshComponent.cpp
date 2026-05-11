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

IMPLEMENT_CLASS(USkeletalMeshComponent, USkinnedMeshComponent)

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


void USkeletalMeshComponent::RefreshBoneTransforms()
{
}

void USkeletalMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
}
