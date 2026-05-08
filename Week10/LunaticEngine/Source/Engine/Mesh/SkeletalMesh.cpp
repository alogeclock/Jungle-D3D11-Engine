#include "SkeletalMesh.h"
#include "Engine/Profiling/MemoryStats.h"

static const FString EmptyPath;

IMPLEMENT_CLASS(USkeletalMesh, UObject)

static uint32 CalculateSkeletalMeshCPUSize(const FSkeletalMesh& Mesh)
{
    uint32 CPUSize = 0;

    CPUSize += static_cast<uint32>(Mesh.Skeleton.Bones.size()) * sizeof(FBoneInfo);

    for (const FSkeletalMeshLOD& LOD : Mesh.LODModels)
    {
        CPUSize += static_cast<uint32>(LOD.Vertices.size()) * sizeof(FSkeletalVertex);
        CPUSize += static_cast<uint32>(LOD.Indices.size()) * sizeof(uint32);
        CPUSize += static_cast<uint32>(LOD.Sections.size()) * sizeof(FStaticMeshSection);
    }

    return CPUSize;
}

USkeletalMesh::~USkeletalMesh()
{
    if (SkeletalMeshAsset)
    {
        const uint32 CPUSize = CalculateSkeletalMeshCPUSize(*SkeletalMeshAsset);
        MemoryStats::SubSkeletalMeshCPUMemory(CPUSize);
    }
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
    else
    {
        FSkeletalMesh EmptyMesh;
        EmptyMesh.Serialize(Ar);
    }

    Ar << SkeletalMaterials;

    if (Ar.IsLoading())
    {
        RebuildSectionMaterialIndices();
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
    if (SkeletalMeshAsset)
    {
        const uint32 OldSize = CalculateSkeletalMeshCPUSize(*SkeletalMeshAsset);
        MemoryStats::SubSkeletalMeshCPUMemory(OldSize);
    }

    SkeletalMeshAsset = InMesh;

    if (SkeletalMeshAsset)
    {
        SkeletalMeshAsset->Skeleton.RebuildChildren();

        for (FSkeletalMeshLOD& LOD : SkeletalMeshAsset->LODModels)
        {
            LOD.CacheBounds();
        }

        const uint32 NewSize = CalculateSkeletalMeshCPUSize(*SkeletalMeshAsset);
        MemoryStats::AddSkeletalMeshCPUMemory(NewSize);
    }

    RebuildSectionMaterialIndices();
}

FSkeletalMesh* USkeletalMesh::GetSkeletalMeshAsset() const
{
    return SkeletalMeshAsset;
}

void USkeletalMesh::SetSkeletalMaterials(TArray<FStaticMaterial>&& InMaterials)
{
    SkeletalMaterials = std::move(InMaterials);
    RebuildSectionMaterialIndices();
}

const TArray<FStaticMaterial>& USkeletalMesh::GetSkeletalMaterials() const
{
    return SkeletalMaterials;
}

void USkeletalMesh::RebuildSectionMaterialIndices()
{
    if (!SkeletalMeshAsset)
    {
        return;
    }

    for (FSkeletalMeshLOD& LOD : SkeletalMeshAsset->LODModels)
    {
        for (FStaticMeshSection& Section : LOD.Sections)
        {
            Section.MaterialIndex = -1;

            for (int32 i = 0; i < static_cast<int32>(SkeletalMaterials.size()); ++i)
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
