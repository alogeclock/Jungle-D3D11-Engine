#include "SkeletalMesh.h"
#include "Object/ObjectFactory.h"
#include "Engine/Profiling/MemoryStats.h"
#include "Render/Resource/Buffer.h"
#include "Render/Types/VertexTypes.h"

#include <cmath>

static const FString EmptyPath;

IMPLEMENT_CLASS(USkeletalMesh, UObject)

namespace
{
    void SanitizeLODInfluences(FSkeletalMeshLOD& LOD, int32 BoneCount)
    {
        for (FSkeletalVertex& Vertex : LOD.Vertices)
        {
            float TotalWeight = 0.0f;
            for (int32 InfluenceIndex = 0; InfluenceIndex < MAX_SKELETAL_MESH_BONE_INFLUENCES; ++InfluenceIndex)
            {
                const bool bValidBone = BoneCount > 0 && Vertex.BoneIndices[InfluenceIndex] < static_cast<uint32>(BoneCount);
                const bool bValidWeight = std::isfinite(Vertex.BoneWeights[InfluenceIndex]) && Vertex.BoneWeights[InfluenceIndex] > 0.0f;
                if (!bValidBone || !bValidWeight)
                {
                    Vertex.BoneIndices[InfluenceIndex] = 0;
                    Vertex.BoneWeights[InfluenceIndex] = 0.0f;
                    continue;
                }

                TotalWeight += Vertex.BoneWeights[InfluenceIndex];
            }

            if (TotalWeight <= 1e-6f)
            {
                for (int32 InfluenceIndex = 0; InfluenceIndex < MAX_SKELETAL_MESH_BONE_INFLUENCES; ++InfluenceIndex)
                {
                    Vertex.BoneIndices[InfluenceIndex] = 0;
                    Vertex.BoneWeights[InfluenceIndex] = 0.0f;
                }
                if (BoneCount > 0)
                {
                    Vertex.BoneWeights[0] = 1.0f;
                }
                continue;
            }

            const float InvTotalWeight = 1.0f / TotalWeight;
            for (int32 InfluenceIndex = 0; InfluenceIndex < MAX_SKELETAL_MESH_BONE_INFLUENCES; ++InfluenceIndex)
            {
                Vertex.BoneWeights[InfluenceIndex] *= InvTotalWeight;
            }
        }
    }

    void SanitizeSkeletalMeshAsset(FSkeletalMesh& Mesh)
    {
        Mesh.Skeleton.SanitizeHierarchyAndBindPose();

        const int32 BoneCount = static_cast<int32>(Mesh.Skeleton.Bones.size());
        for (FSkeletalMeshLOD& LOD : Mesh.LODModels)
        {
            SanitizeLODInfluences(LOD, BoneCount);
            LOD.CacheBounds();
        }
    }
}

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

    for (const FSkeletalAnimationClip& Clip : Mesh.Animations)
    {
        CPUSize += sizeof(FSkeletalAnimationClip);
        CPUSize += static_cast<uint32>(Clip.Tracks.size()) * sizeof(FBoneAnimationTrack);

        for (const FBoneAnimationTrack& Track : Clip.Tracks)
        {
            CPUSize += static_cast<uint32>(Track.Keys.size()) * sizeof(FBoneTransformKey);
        }
    }

    for (const FMorphTarget& Morph : Mesh.MorphTargets)
    {
        CPUSize += sizeof(FMorphTarget);

        for (const FMorphTargetLOD& LOD : Morph.LODModels)
        {
            CPUSize += static_cast<uint32>(LOD.Shapes.size()) * sizeof(FMorphTargetShape);

            for (const FMorphTargetShape& Shape : LOD.Shapes)
            {
                CPUSize += static_cast<uint32>(Shape.Deltas.size()) * sizeof(FMorphTargetDelta);
            }
        }
    }

    return CPUSize;
}

USkeletalMesh::~USkeletalMesh()
{
    if (SkeletalMeshAsset)
    {
        const uint32 CPUSize = CalculateSkeletalMeshCPUSize(*SkeletalMeshAsset);
        MemoryStats::SubSkeletalMeshCPUMemory(CPUSize);

        delete SkeletalMeshAsset;
        SkeletalMeshAsset = nullptr;
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
    if (SkeletalMeshAsset == InMesh)
    {
        if (SkeletalMeshAsset)
        {
            SanitizeSkeletalMeshAsset(*SkeletalMeshAsset);
        }

        RebuildSectionMaterialIndices();
        RenderBuffer.reset();
        return;
    }

    if (SkeletalMeshAsset)
    {
        const uint32 OldSize = CalculateSkeletalMeshCPUSize(*SkeletalMeshAsset);
        MemoryStats::SubSkeletalMeshCPUMemory(OldSize);

        delete SkeletalMeshAsset;
        SkeletalMeshAsset = nullptr;
    }

    SkeletalMeshAsset = InMesh;

    if (SkeletalMeshAsset)
    {
        SanitizeSkeletalMeshAsset(*SkeletalMeshAsset);

        const uint32 NewSize = CalculateSkeletalMeshCPUSize(*SkeletalMeshAsset);
        MemoryStats::AddSkeletalMeshCPUMemory(NewSize);
    }

    RebuildSectionMaterialIndices();
    RenderBuffer.reset();
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

void USkeletalMesh::InitResources(ID3D11Device* InDevice)
{
    RenderBuffer.reset();

    if (!InDevice || !SkeletalMeshAsset)
    {
        return;
    }

    const FSkeletalMeshLOD* LOD = SkeletalMeshAsset->GetLOD(0);
    if (!LOD || LOD->Vertices.empty())
    {
        return;
    }

    TMeshData<FVertexPNCTT> RenderMeshData;
    RenderMeshData.Vertices.reserve(LOD->Vertices.size());

    for (const FSkeletalVertex& RawVertex : LOD->Vertices)
    {
        FVertexPNCTT RenderVertex;
        RenderVertex.Position = RawVertex.Pos;
        RenderVertex.Normal = RawVertex.Normal;
        RenderVertex.Color = RawVertex.Color;
        RenderVertex.UV = RawVertex.UV[0];
        RenderVertex.Tangent = RawVertex.Tangent;
        RenderMeshData.Vertices.push_back(RenderVertex);
    }
    RenderMeshData.Indices = LOD->Indices;

    RenderBuffer = std::make_unique<FMeshBuffer>();
    RenderBuffer->Create(InDevice, RenderMeshData);
}

FMeshBuffer* USkeletalMesh::GetMeshBuffer() const
{
    return RenderBuffer.get();
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
