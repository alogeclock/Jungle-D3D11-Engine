#include "Render/Proxy/SkeletalMeshSceneProxy.h"

#include <algorithm>

#include "Component/SkinnedMeshComponent.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"

namespace
{
	bool SectionMaterialLess(const FMeshSectionDraw& A, const FMeshSectionDraw& B)
	{
		const uintptr_t AMat = reinterpret_cast<uintptr_t>(A.Material);
		const uintptr_t BMat = reinterpret_cast<uintptr_t>(B.Material);
		if (AMat != BMat)
			return AMat < BMat;

		return A.FirstIndex < B.FirstIndex;
	}

	void SortSectionDrawsByMaterial(TArray<FMeshSectionDraw>& Draws)
	{
		if (Draws.size() > 1)
		{
			std::sort(Draws.begin(), Draws.end(), SectionMaterialLess);
		}
	}
}

FSkeletalMeshSceneProxy::FSkeletalMeshSceneProxy(USkinnedMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags |= EPrimitiveProxyFlags::SkeletalMesh;
}

USkinnedMeshComponent* FSkeletalMeshSceneProxy::GetSkinnedMeshComponent() const
{
	return static_cast<USkinnedMeshComponent*>(GetOwner());
}

void FSkeletalMeshSceneProxy::UpdateMaterial()
{
	RebuildSectionDraws();
}

void FSkeletalMeshSceneProxy::UpdateMesh()
{
	MeshBuffer = GetOwner()->GetMeshBuffer();
	RebuildSectionDraws();
}

void FSkeletalMeshSceneProxy::UpdateLOD(uint32 LODLevel)
{
	if (LODLevel >= LODCount) LODLevel = LODCount - 1;
	if (LODLevel == CurrentLOD) return;

	if (USkinnedMeshComponent* SMC = GetSkinnedMeshComponent())
	{
		SMC->SetRenderLOD(LODLevel);
		MeshBuffer = SMC->GetMeshBuffer();
	}
	CurrentLOD = LODLevel;
	RebuildSectionDraws();
}

void FSkeletalMeshSceneProxy::RebuildSectionDraws()
{
	USkinnedMeshComponent* SMC = GetSkinnedMeshComponent();
	USkeletalMesh* Mesh = SMC ? SMC->GetSkeletalMesh() : nullptr;
	FSkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;

	if (!Asset || Asset->LODModels.empty())
	{
		for (uint32 lod = 0; lod < MAX_LOD; ++lod)
		{
			LODData[lod].MeshBuffer = nullptr;
			LODData[lod].SectionDraws.clear();
		}

		LODCount = 1;
		CurrentLOD = 0;
		MeshBuffer = nullptr;
		SectionDraws.clear();

		return;
	}

	const auto& Slots = Mesh->GetSkeletalMaterials();
	const auto& Overrides = SMC->GetOverrideMaterials();
	UMaterial* FallbackMaterial = nullptr;

	auto ResolveMaterialForSection = [&](const FStaticMeshSection& Section) -> UMaterial*
	{
		const int32 SectionMaterialIndex = Section.MaterialIndex;
		if (SectionMaterialIndex >= 0 && SectionMaterialIndex < static_cast<int32>(Slots.size()))
		{
			if (SectionMaterialIndex < static_cast<int32>(Overrides.size()) && Overrides[SectionMaterialIndex])
			{
				return Overrides[SectionMaterialIndex];
			}
			if (Slots[SectionMaterialIndex].MaterialInterface)
			{
				return Slots[SectionMaterialIndex].MaterialInterface;
			}
		}

		if (!Overrides.empty() && Overrides[0])
		{
			return Overrides[0];
		}
		if (!Slots.empty() && Slots[0].MaterialInterface)
		{
			return Slots[0].MaterialInterface;
		}

		if (!FallbackMaterial)
		{
			FallbackMaterial = FMaterialManager::Get().GetOrCreateMaterial("None");
		}
		return FallbackMaterial;
	};

	LODCount = std::min<uint32>(static_cast<uint32>(Asset->LODModels.size()), MAX_LOD);
	if (LODCount == 0)
	{
		LODCount = 1;
	}
	if (CurrentLOD >= LODCount)
	{
		CurrentLOD = LODCount - 1;
	}

	const FSkeletalMeshLOD& LOD0 = Asset->LODModels[CurrentLOD];
	SectionDraws.clear();
	SectionDraws.reserve(LOD0.Sections.size());

	if (LOD0.Sections.empty() && MeshBuffer)
	{
		FMeshSectionDraw Draw;
		Draw.FirstIndex = 0;
		Draw.IndexCount = MeshBuffer->GetIndexBuffer().GetIndexCount();
		Draw.Material = !Overrides.empty() && Overrides[0]
			? Overrides[0]
			: (!Slots.empty() && Slots[0].MaterialInterface ? Slots[0].MaterialInterface : FMaterialManager::Get().GetOrCreateMaterial("None"));
		SectionDraws.push_back(Draw);
		return;
	}

	for (const FStaticMeshSection& Section : LOD0.Sections)
	{
		FMeshSectionDraw Draw;
		Draw.FirstIndex = Section.FirstIndex;
		Draw.IndexCount = Section.NumTriangles * 3;
		Draw.Material = ResolveMaterialForSection(Section);
		SectionDraws.push_back(Draw);
	}
	SortSectionDrawsByMaterial(SectionDraws);
}
