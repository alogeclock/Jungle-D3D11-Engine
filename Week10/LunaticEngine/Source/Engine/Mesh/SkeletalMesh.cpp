#include "Mesh/SkeletalMesh.h"

#include "Object/ObjectFactory.h"

IMPLEMENT_CLASS(USkeletalMesh, UObject)

static const FString EmptyPath;

USkeletalMesh::~USkeletalMesh()
{
	delete SkeletalMeshAsset;
	SkeletalMeshAsset = nullptr;
}

void USkeletalMesh::Serialize(FArchive& Ar)
{
	if (Ar.IsLoading() && !SkeletalMeshAsset)
	{
		SkeletalMeshAsset = new FSkeletalMesh();
	}

	if (SkeletalMeshAsset)
	{
		SkeletalMeshAsset->Serialize(Ar);
	}

	Ar << SkeletalMaterials;

	if (Ar.IsLoading() && SkeletalMeshAsset)
	{
		for (FSkeletalMeshLOD& LOD : SkeletalMeshAsset->LODModels)
		{
			for (FStaticMeshSection& Section : LOD.Sections)
			{
				Section.MaterialIndex = -1;
				for (int32 i = 0; i < (int32)SkeletalMaterials.size(); ++i)
				{
					if (SkeletalMaterials[i].MaterialSlotName == Section.MaterialSlotName)
					{
						Section.MaterialIndex = i;
						break;
					}
				}
			}
		}
	}
}

const FString& USkeletalMesh::GetAssetPathFileName() const
{
	if (SkeletalMeshAsset)
	{
		return SkeletalMeshAsset->PathFileName;
	}
	return EmptyPath;
}

void USkeletalMesh::SetSkeletalMeshAsset(FSkeletalMesh* InMesh)
{
	SkeletalMeshAsset = InMesh;

	if (!SkeletalMeshAsset)
	{
		return;
	}

	for (FSkeletalMeshLOD& LOD : SkeletalMeshAsset->LODModels)
	{
		for (FStaticMeshSection& Section : LOD.Sections)
		{
			Section.MaterialIndex = -1;
			for (int32 i = 0; i < (int32)SkeletalMaterials.size(); ++i)
			{
				if (SkeletalMaterials[i].MaterialSlotName == Section.MaterialSlotName)
				{
					Section.MaterialIndex = i;
					break;
				}
			}
		}
	}
}

void USkeletalMesh::SetSkeletalMaterials(TArray<FStaticMaterial>&& InMaterials)
{
	SkeletalMaterials = std::move(InMaterials);
}
