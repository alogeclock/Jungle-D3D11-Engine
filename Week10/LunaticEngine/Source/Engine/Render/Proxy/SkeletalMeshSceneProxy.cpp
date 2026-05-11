#include "Render/Proxy/SkeletalMeshSceneProxy.h"

#include "Component/SkeletalMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"

#include <algorithm>

namespace
{
	bool SectionMaterialLess(const FMeshSectionDraw& A, const FMeshSectionDraw& B);
	void SortSectionDrawsByMaterial(TArray<FMeshSectionDraw>& Draws);
}

FSkeletalMeshSceneProxy::FSkeletalMeshSceneProxy(USkeletalMeshComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
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
	if (LODLevel >= LODCount)
	{
		LODLevel = LODCount - 1;
	}
	if (LODLevel == CurrentLOD)
	{
		return;
	}

	std::swap(MeshBuffer, LODData[CurrentLOD].MeshBuffer);
	std::swap(SectionDraws, LODData[CurrentLOD].SectionDraws);

	CurrentLOD = LODLevel;
	std::swap(MeshBuffer, LODData[LODLevel].MeshBuffer);
	std::swap(SectionDraws, LODData[LODLevel].SectionDraws);
}

USkeletalMeshComponent* FSkeletalMeshSceneProxy::GetSkeletalMeshComponent() const
{
	return static_cast<USkeletalMeshComponent*>(GetOwner());
}

void FSkeletalMeshSceneProxy::RebuildSectionDraws()
{
	USkeletalMeshComponent* SMC = GetSkeletalMeshComponent();
	USkeletalMesh* Mesh = SMC ? SMC->GetSkeletalMeshAsset() : nullptr;
	const FSkeletalMesh* MeshAsset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!SMC || !Mesh || !MeshAsset)
	{
		for (uint32 LODIndex = 0; LODIndex < MAX_LOD; ++LODIndex)
		{
			LODData[LODIndex].MeshBuffer = nullptr;
			LODData[LODIndex].SectionDraws.clear();
		}

		LODCount = 1;
		CurrentLOD = 0;
		MeshBuffer = nullptr;
		SectionDraws.clear();
		return;
	}

	const TArray<FStaticMaterial>& Slots = Mesh->GetSkeletalMaterials();
	const TArray<UMaterial*>& Overrides = SMC->GetOverrideMaterials();
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

	LODCount = 1;

	for (uint32 LODIndex = 0; LODIndex < MAX_LOD; ++LODIndex)
	{
		LODData[LODIndex].MeshBuffer = nullptr;
		LODData[LODIndex].SectionDraws.clear();
	}

	for (uint32 LODIndex = 0; LODIndex < LODCount; ++LODIndex)
	{
		const FSkeletalMeshLOD* LOD = MeshAsset->GetLOD(static_cast<int32>(LODIndex));
		LODData[LODIndex].MeshBuffer = SMC->GetMeshBuffer();
		if (!LOD)
		{
			continue;
		}

		if (LOD->Sections.empty() && LODData[LODIndex].MeshBuffer)
		{
			FMeshSectionDraw Draw;
			Draw.Material = !Overrides.empty() && Overrides[0]
				? Overrides[0]
				: (!Slots.empty() && Slots[0].MaterialInterface ? Slots[0].MaterialInterface : FMaterialManager::Get().GetOrCreateMaterial("None"));
			if (Draw.Material)
			{
				Draw.Material->RebuildCachedSRVs();
			}
			Draw.FirstIndex = 0;
			Draw.IndexCount = LODData[LODIndex].MeshBuffer->GetIndexBuffer().GetIndexCount();
			LODData[LODIndex].SectionDraws.push_back(Draw);
			continue;
		}

		for (const FStaticMeshSection& Section : LOD->Sections)
		{
			FMeshSectionDraw Draw;
			Draw.FirstIndex = Section.FirstIndex;
			Draw.IndexCount = Section.NumTriangles * 3;
			Draw.Material = ResolveMaterialForSection(Section);
			if (Draw.Material)
			{
				Draw.Material->RebuildCachedSRVs();
			}
			LODData[LODIndex].SectionDraws.push_back(Draw);
		}

		SortSectionDrawsByMaterial(LODData[LODIndex].SectionDraws);
	}

	CurrentLOD = 0;
	MeshBuffer = LODData[0].MeshBuffer;
	SectionDraws = LODData[0].SectionDraws;
}

namespace
{
	bool SectionMaterialLess(const FMeshSectionDraw& A, const FMeshSectionDraw& B)
	{
		const uintptr_t AMat = reinterpret_cast<uintptr_t>(A.Material);
		const uintptr_t BMat = reinterpret_cast<uintptr_t>(B.Material);
		if (AMat != BMat)
		{
			return AMat < BMat;
		}

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
