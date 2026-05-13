#include "Mesh/Fbx/FbxSkeletalMeshLODBuilder.h"

#include "Mesh/Fbx/FbxGeometryReader.h"
#include "Mesh/Fbx/FbxImportContext.h"
#include "Mesh/Fbx/FbxMaterialImporter.h"
#include "Mesh/Fbx/FbxSectionBuilder.h"
#include "Mesh/Fbx/FbxSkeletonImporter.h"
#include "Mesh/Fbx/FbxSkinWeightImporter.h"
#include "Mesh/Fbx/FbxTransformUtils.h"
#include "Mesh/Fbx/FbxVertexDeduplicator.h"

#include <fbxsdk.h>

#include <algorithm>
#include <cstddef>

bool FFbxSkeletalMeshLODBuilder::Build(
    FbxScene*                                 Scene,
    const FString&                            SourcePath,
    const TArray<FFbxSkeletalImportMeshNode>& MeshNodes,
    const TMap<FbxNode*, int32>&              BoneNodeToIndex,
    const FMatrix&                            ReferenceMeshBindInverse,
    const FSkeleton&                          Skeleton,
    TArray<FStaticMaterial>&                  OutMaterials,
    FSkeletalMeshLOD&                         OutLOD,
    TArray<FFbxImportedMorphSourceVertex>*    OutMorphSources,
    FFbxImportContext&                        BuildContext
    )
{
    OutLOD = FSkeletalMeshLOD();

    if (OutMorphSources)
    {
        OutMorphSources->clear();
    }

    FFbxSectionBuilder             SectionBuilder;
    FFbxSkeletalVertexDeduplicator VertexDeduplicator;

    for (const FFbxSkeletalImportMeshNode& ImportNode : MeshNodes)
    {
        FbxNode* MeshNode = ImportNode.MeshNode;
        FbxMesh* Mesh     = MeshNode ? MeshNode->GetMesh() : nullptr;

        const bool bMergeStaticChild = ImportNode.Kind == EFbxSkeletalImportMeshKind::StaticChildOfBone && ImportNode.StaticChildAction ==
        ESkeletalStaticChildImportAction::MergeAsRigidPart;

        if (!Mesh || ImportNode.Kind == EFbxSkeletalImportMeshKind::Loose || ImportNode.Kind == EFbxSkeletalImportMeshKind::Ignored || ImportNode.Kind ==
            EFbxSkeletalImportMeshKind::CollisionProxy || (ImportNode.Kind == EFbxSkeletalImportMeshKind::StaticChildOfBone && !bMergeStaticChild))
        {
            continue;
        }

        TArray<FString> MeshUVSetNames;
        FFbxGeometryReader::GetUVSetNames(Mesh, MeshUVSetNames);
        for (int32 UVSetIndex = 0; UVSetIndex < static_cast<int32>(MeshUVSetNames.size()) && OutLOD.UVSetNames.size() < MAX_SKELETAL_MESH_UV_CHANNELS; ++UVSetIndex)
        {
            const FString& UVSetName = MeshUVSetNames[UVSetIndex];
            if (std::find(OutLOD.UVSetNames.begin(), OutLOD.UVSetNames.end(), UVSetName) == OutLOD.UVSetNames.end())
            {
                OutLOD.UVSetNames.push_back(UVSetName);
            }
        }

        FMatrix MeshToReference;

        if (ImportNode.Kind == EFbxSkeletalImportMeshKind::Skinned)
        {
            FMatrix MeshBind;
            if (!FFbxSkeletonImporter::TryGetFirstMeshBindMatrix(Scene, MeshNode, MeshBind))
            {
                continue;
            }
            MeshToReference = MeshBind * ReferenceMeshBindInverse;
        }
        else if (bMergeStaticChild)
        {
            if (!ImportNode.ParentBoneNode || ImportNode.ParentBoneIndex < 0 || ImportNode.ParentBoneIndex >= static_cast<int32>(Skeleton.Bones.size()))
            {
                continue;
            }

            MeshToReference = ImportNode.LocalMatrixToParentBone * Skeleton.Bones[ImportNode.ParentBoneIndex].GlobalBindPose;
        }

        const FMatrix NormalToReference = MeshToReference.GetInverse().GetTransposed();
        const bool    bReverseWinding   = FFbxTransformUtils::Determinant3x3(MeshToReference) < 0.0f;

        TArray<TArray<FFbxImportedBoneWeight>> ControlPointWeight;
        if (ImportNode.Kind == EFbxSkeletalImportMeshKind::Skinned)
        {
            FFbxSkinWeightImporter::ExtractSkinWeightsOnly(Mesh, BoneNodeToIndex, ControlPointWeight, BuildContext);
        }

        int32 PolygonVertexIndex = 0;

        for (int32 PolygonIndex = 0; PolygonIndex < Mesh->GetPolygonCount(); ++PolygonIndex)
        {
            const int32 PolygonSize = Mesh->GetPolygonSize(PolygonIndex);

            if (PolygonSize != 3)
            {
                PolygonVertexIndex += PolygonSize;
                continue;
            }

            const int32 LocalMaterialIndex = FFbxMaterialSlotResolver::GetPolygonMaterialIndex(Mesh, PolygonIndex);

            FbxSurfaceMaterial* FbxMat = nullptr;

            if (LocalMaterialIndex >= 0 && LocalMaterialIndex < MeshNode->GetMaterialCount())
            {
                FbxMat = MeshNode->GetMaterial(LocalMaterialIndex);
            }

            const int32 MaterialIndex = FFbxMaterialImporter::FindOrAddMaterial(FbxMat, SourcePath, OutMaterials, BuildContext);

            FFbxImportedSectionBuild* SectionBuild = SectionBuilder.FindOrAddSection(MaterialIndex);

            int32    ControlPointIndices[3] = {};
            FVector  Positions[3];
            FVector2 UV0[3];

            for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
            {
                ControlPointIndices[CornerIndex] = Mesh->GetPolygonVertex(PolygonIndex, CornerIndex);

                const FVector LocalPosition = FFbxGeometryReader::ReadPosition(Mesh, ControlPointIndices[CornerIndex]);

                Positions[CornerIndex] = FFbxTransformUtils::TransformPositionByMatrix(LocalPosition, MeshToReference);

                UV0[CornerIndex] = MeshUVSetNames.empty()
                    ? FVector2(0.0f, 0.0f)
                    : FFbxGeometryReader::ReadUVByName(Mesh, PolygonIndex, CornerIndex, MeshUVSetNames[0].c_str());
            }

            FVector       FallbackNormal  = FFbxGeometryReader::ComputeTriangleNormal(Positions[0], Positions[1], Positions[2]);
            const FVector FallbackTangent = FFbxGeometryReader::ComputeTriangleTangent(Positions[0], Positions[1], Positions[2], UV0[0], UV0[1], UV0[2]);

            if (bReverseWinding)
            {
                FallbackNormal *= -1.0f;
            }

            uint32 TriangleVertexIndices[3] = {};

            for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
            {
                const int32 ControlPointIndex = ControlPointIndices[CornerIndex];

                const int32 CurrentPolygonVertexIndex = PolygonVertexIndex + CornerIndex;

                FSkeletalVertex Vertex;

                bool                      bGeneratedNormal  = false;
                bool                      bGeneratedTangent = false;
                bool                      bMissingUV        = false;
                FFbxPackedBoneWeightStats BoneWeightStats;

                Vertex.Pos = Positions[CornerIndex];

                FVector LocalNormal;
                if (FFbxGeometryReader::TryReadNormal(Mesh, PolygonIndex, CornerIndex, LocalNormal))
                {
                    Vertex.Normal = FFbxTransformUtils::TransformNormalByMatrix(LocalNormal, NormalToReference);
                }
                else
                {
                    Vertex.Normal    = FallbackNormal;
                    bGeneratedNormal = true;
                }

                const int32 RawUVSetCount = static_cast<int32>(MeshUVSetNames.size());

                if (RawUVSetCount <= 0)
                {
                    bMissingUV = true;
                }

                const int32 UVCount = static_cast<int32>((std::min<std::size_t>)(
                    static_cast<std::size_t>(MAX_SKELETAL_MESH_UV_CHANNELS),
                    static_cast<std::size_t>(RawUVSetCount)
                ));

                Vertex.NumUVs = static_cast<uint8>(UVCount > 0 ? UVCount : 1);

                if (RawUVSetCount <= 0)
                {
                    Vertex.UV[0] = FVector2(0.0f, 0.0f);
                }

                for (int32 UVIndex = 0; UVIndex < UVCount; ++UVIndex)
                {
                    Vertex.UV[UVIndex] = FFbxGeometryReader::ReadUVByName(Mesh, PolygonIndex, CornerIndex, MeshUVSetNames[UVIndex].c_str());
                }

                Vertex.Color = FFbxGeometryReader::ReadVertexColor(Mesh, ControlPointIndex, CurrentPolygonVertexIndex);

                FVector4 ImportedTangent;
                if (FFbxGeometryReader::TryReadTangent(Mesh, ControlPointIndex, CurrentPolygonVertexIndex, ImportedTangent))
                {
                    const FVector T = FFbxTransformUtils::TransformTangentByMatrix(
                        FVector(ImportedTangent.X, ImportedTangent.Y, ImportedTangent.Z),
                        MeshToReference,
                        Vertex.Normal
                    );

                    Vertex.Tangent = FVector4(T.X, T.Y, T.Z, bReverseWinding ? -ImportedTangent.W : ImportedTangent.W);
                }
                else
                {
                    const FVector T = FFbxTransformUtils::OrthogonalizeTangentToNormal(FallbackTangent, Vertex.Normal);

                    Vertex.Tangent    = FVector4(T.X, T.Y, T.Z, bReverseWinding ? -1.0f : 1.0f);
                    bGeneratedTangent = true;
                }

                if (bMergeStaticChild)
                {
                    FFbxSkinWeightImporter::SetRigidBoneWeight(ImportNode.ParentBoneIndex, Vertex.BoneIndices, Vertex.BoneWeights, BoneWeightStats);
                }
                else if (ControlPointIndex >= 0 && ControlPointIndex < static_cast<int32>(ControlPointWeight.size()))
                {
                    FFbxSkinWeightImporter::PackTopBoneWeights(ControlPointWeight[ControlPointIndex], Vertex.BoneIndices, Vertex.BoneWeights, BoneWeightStats);
                }
                else
                {
                    TArray<FFbxImportedBoneWeight> EmptyWeight;
                    FFbxSkinWeightImporter::PackTopBoneWeights(EmptyWeight, Vertex.BoneIndices, Vertex.BoneWeights, BoneWeightStats);
                }

                BuildContext.Summary.CandidateVertexCount++;

                bool bAddedNewVertex = false;

                const uint32 VertexIndex = VertexDeduplicator.FindOrAdd(Vertex, Mesh, ControlPointIndex, MaterialIndex, OutLOD.Vertices, bAddedNewVertex);

                if (bAddedNewVertex)
                {
                    FFbxSkinWeightImporter::CommitUniqueVertexImportStats(BuildContext, bGeneratedNormal, bGeneratedTangent, bMissingUV, BoneWeightStats);
                }
                else
                {
                    BuildContext.Summary.DeduplicatedVertexCount++;
                }

                TriangleVertexIndices[CornerIndex] = VertexIndex;

                if (OutMorphSources && bAddedNewVertex)
                {
                    FFbxImportedMorphSourceVertex Source;
                    Source.MeshNode               = MeshNode;
                    Source.Mesh                   = Mesh;
                    Source.SourceMeshNodeName     = MeshNode ? FString(MeshNode->GetName()) : FString();
                    Source.ControlPointIndex      = ControlPointIndex;
                    Source.PolygonIndex           = PolygonIndex;
                    Source.CornerIndex            = CornerIndex;
                    Source.PolygonVertexIndex     = CurrentPolygonVertexIndex;
                    Source.VertexIndex            = VertexIndex;
                    Source.MeshToReference        = MeshToReference;
                    Source.NormalToReference      = NormalToReference;
                    Source.BaseNormalInReference  = Vertex.Normal;
                    Source.BaseTangentInReference = Vertex.Tangent;

                    OutMorphSources->push_back(Source);
                }
            }

            if (bReverseWinding)
            {
                SectionBuild->Indices.push_back(TriangleVertexIndices[0]);
                SectionBuild->Indices.push_back(TriangleVertexIndices[2]);
                SectionBuild->Indices.push_back(TriangleVertexIndices[1]);
            }
            else
            {
                SectionBuild->Indices.push_back(TriangleVertexIndices[0]);
                SectionBuild->Indices.push_back(TriangleVertexIndices[1]);
                SectionBuild->Indices.push_back(TriangleVertexIndices[2]);
            }

            BuildContext.Summary.TriangleCount++;
            PolygonVertexIndex += PolygonSize;
        }
    }

    if (OutLOD.Vertices.empty() || SectionBuilder.IsEmpty())
    {
        return false;
    }

    SectionBuilder.BuildFinalSections(OutMaterials, OutLOD.Indices, OutLOD.Sections);

    if (OutLOD.Indices.empty() || OutLOD.Sections.empty())
    {
        return false;
    }

    OutLOD.CacheBounds();

    return true;
}
