#include "Mesh/Fbx/FbxStaticMeshImporter.h"

#include "Mesh/Fbx/FbxCollisionImporter.h"
#include "Mesh/Fbx/FbxGeometryReader.h"
#include "Mesh/Fbx/FbxImportContext.h"
#include "Mesh/Fbx/FbxMaterialImporter.h"
#include "Mesh/Fbx/FbxMetadataImporter.h"
#include "Mesh/Fbx/FbxSceneHierarchyImporter.h"
#include "Mesh/Fbx/FbxSceneQuery.h"
#include "Mesh/Fbx/FbxSectionBuilder.h"
#include "Mesh/Fbx/FbxTransformUtils.h"
#include "Mesh/Fbx/FbxVertexDeduplicator.h"
#include "Mesh/StaticMeshAsset.h"

#include <algorithm>
#include <fbxsdk.h>

namespace
{
    static FStaticMeshLOD MakeLODFromMesh(const FStaticMesh& Mesh, int32 SourceLODIndex, float ScreenSize, float DistanceThreshold)
    {
        FStaticMeshLOD LOD;
        LOD.SourceLODIndex    = SourceLODIndex;
        LOD.SourceLODName     = "LOD" + std::to_string(SourceLODIndex);
        LOD.ScreenSize        = ScreenSize;
        LOD.DistanceThreshold = DistanceThreshold;
        LOD.Vertices          = Mesh.Vertices;
        LOD.Indices           = Mesh.Indices;
        LOD.Sections          = Mesh.Sections;
        LOD.CacheBounds();
        return LOD;
    }

    static void AppendStaticCollisionNode(FStaticMesh& MeshAsset, FbxNode* MeshNode)
    {
        const FFbxMeshImportSpace ImportSpace = FFbxMeshImportSpace::FromStaticMeshNode(MeshNode);

        FImportedCollisionShape Shape;
        if (FFbxCollisionImporter::ImportCollisionShape(MeshNode, ImportSpace.VertexTransform, -1, FString(), Shape))
        {
            MeshAsset.CollisionShapes.push_back(Shape);
        }
    }

    static FFbxMeshImportSpace MakeLocalMeshImportSpace(FbxNode* MeshNode)
    {
        FFbxMeshImportSpace Result;
        if (!MeshNode)
        {
            return Result;
        }

        Result.VertexTransform = FFbxTransformUtils::ToEngineMatrix(FFbxTransformUtils::GetNodeGeometryTransform(MeshNode));
        Result.NormalTransform = Result.VertexTransform.GetInverse().GetTransposed();
        return Result;
    }
}

class FFbxStaticMeshBuilder
{
public:
    FFbxStaticMeshBuilder(
        const FString&           InSourcePath,
        TArray<FStaticMaterial>& InOutMaterials,
        FStaticMesh&             InOutMesh,
        FFbxImportContext&       InBuildContext,
        bool                     bInBakeNodeGlobalTransform = true
        )
        : SourcePath(InSourcePath), Materials(InOutMaterials), MeshAsset(InOutMesh), BuildContext(InBuildContext),
          bBakeNodeGlobalTransform(bInBakeNodeGlobalTransform)
    {}

    // Build 함수의 FBX import 내부 처리 단계를 수행한다.
    bool Build(const TArray<FbxNode*>& MeshNodes)
    {
        ResetOutput();

        for (FbxNode* MeshNode : MeshNodes)
        {
            if (FFbxSceneQuery::IsCollisionProxyNode(MeshNode))
            {
                AppendCollisionNode(MeshNode);
                continue;
            }
            AppendMeshNode(MeshNode);
        }

        return Finalize();
    }

private:
    const FString&           SourcePath;
    TArray<FStaticMaterial>& Materials;
    FStaticMesh&             MeshAsset;
    FFbxImportContext&       BuildContext;
    bool                     bBakeNodeGlobalTransform = true;

    FFbxSectionBuilder           SectionBuilder;
    FFbxStaticVertexDeduplicator VertexDeduplicator;

    // ResetOutput 함수의 FBX import 내부 처리 단계를 수행한다.
    void ResetOutput()
    {
        MeshAsset.Vertices.clear();
        MeshAsset.Indices.clear();
        MeshAsset.Sections.clear();
        MeshAsset.LODModels.clear();
        MeshAsset.CollisionShapes.clear();
        SectionBuilder     = FFbxSectionBuilder();
        VertexDeduplicator = FFbxStaticVertexDeduplicator();
    }

    // AppendMeshNode 함수의 FBX import 내부 처리 단계를 수행한다.
    void AppendMeshNode(FbxNode* MeshNode)
    {
        FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;
        if (!Mesh)
        {
            return;
        }

        const FFbxMeshImportSpace ImportSpace = bBakeNodeGlobalTransform ? FFbxMeshImportSpace::FromStaticMeshNode(MeshNode)
        : MakeLocalMeshImportSpace(MeshNode);

        TArray<FString> UVSetNames;
        FFbxGeometryReader::GetUVSetNames(Mesh, UVSetNames);

        int32 PolygonVertexIndex = 0;

        for (int32 PolygonIndex = 0; PolygonIndex < Mesh->GetPolygonCount(); ++PolygonIndex)
        {
            const int32 PolygonSize = Mesh->GetPolygonSize(PolygonIndex);

            if (PolygonSize == 3)
            {
                AppendTriangle(MeshNode, Mesh, PolygonIndex, PolygonVertexIndex, ImportSpace, UVSetNames);
            }

            PolygonVertexIndex += PolygonSize;
        }
    }

    // AppendTriangle 함수의 FBX import 내부 처리 단계를 수행한다.
    void AppendTriangle(
        FbxNode*                   MeshNode,
        FbxMesh*                   Mesh,
        int32                      PolygonIndex,
        int32                      PolygonVertexStartIndex,
        const FFbxMeshImportSpace& ImportSpace,
        const TArray<FString>&     UVSetNames
        )
    {
        FFbxTriangleSample Triangle;
        const char* UV0SetName = UVSetNames.empty() ? nullptr : UVSetNames[0].c_str();
        if (!FFbxGeometryReader::ReadTriangleSample(Mesh, PolygonIndex, ImportSpace, UV0SetName, Triangle))
        {
            return;
        }

        const int32               MaterialIndex = ResolveMaterialIndex(MeshNode, Mesh, PolygonIndex);
        FFbxImportedSectionBuild* SectionBuild  = SectionBuilder.FindOrAddSection(MaterialIndex);
        if (!SectionBuild)
        {
            return;
        }

        for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
        {
            const uint32 VertexIndex = AddCornerVertex(Mesh, PolygonIndex, CornerIndex, PolygonVertexStartIndex + CornerIndex, Triangle, ImportSpace, UVSetNames);

            SectionBuild->Indices.push_back(VertexIndex);
        }
    }

    // ResolveMaterialIndex 함수의 FBX import 내부 처리 단계를 수행한다.
    int32 ResolveMaterialIndex(FbxNode* MeshNode, FbxMesh* Mesh, int32 PolygonIndex)
    {
        FbxSurfaceMaterial* FbxMaterial = FFbxMaterialSlotResolver::ResolvePolygonMaterial(MeshNode, Mesh, PolygonIndex);
        return FFbxMaterialImporter::FindOrAddMaterial(FbxMaterial, SourcePath, Materials, BuildContext);
    }

    uint32 AddCornerVertex(
        FbxMesh*                   Mesh,
        int32                      PolygonIndex,
        int32                      CornerIndex,
        int32                      PolygonVertexIndex,
        const FFbxTriangleSample&  Triangle,
        const FFbxMeshImportSpace& ImportSpace,
        const TArray<FString>&     UVSetNames
        )
    {
        const int32 ControlPointIndex = Triangle.ControlPointIndices[CornerIndex];

        FNormalVertex Vertex;
        Vertex.pos     = Triangle.Positions[CornerIndex];
        Vertex.normal  = ReadCornerNormal(Mesh, PolygonIndex, CornerIndex, Triangle, ImportSpace);
        Vertex.tex     = Triangle.UV0[CornerIndex];
        Vertex.color   = FFbxGeometryReader::ReadVertexColor(Mesh, ControlPointIndex, PolygonVertexIndex);
        Vertex.tangent = ReadCornerTangent(Mesh, ControlPointIndex, PolygonVertexIndex, Triangle, ImportSpace, Vertex.normal);

        const int32 RawUVSetCount = static_cast<int32>(UVSetNames.size());
        const int32 UVCount       = (std::min)(RawUVSetCount, static_cast<int32>(MAX_STATIC_MESH_UV_CHANNELS));
        Vertex.NumUVs             = static_cast<uint8>(UVCount > 0 ? UVCount : 1);

        for (int32 UVIndex = 1; UVIndex < static_cast<int32>(Vertex.NumUVs); ++UVIndex)
        {
            Vertex.ExtraUV[UVIndex - 1] = FFbxGeometryReader::ReadUVByName(Mesh, PolygonIndex, CornerIndex, UVSetNames[UVIndex].c_str());
        }

        bool bAddedNewVertex = false;
        return VertexDeduplicator.FindOrAdd(Vertex, Mesh, ControlPointIndex, MeshAsset.Vertices, bAddedNewVertex);
    }

    // ReadCornerNormal 함수의 FBX import 내부 처리 단계를 수행한다.
    FVector ReadCornerNormal(
        FbxMesh*                   Mesh,
        int32                      PolygonIndex,
        int32                      CornerIndex,
        const FFbxTriangleSample&  Triangle,
        const FFbxMeshImportSpace& ImportSpace
        ) const
    {
        FVector LocalNormal;
        if (FFbxGeometryReader::TryReadNormal(Mesh, PolygonIndex, CornerIndex, LocalNormal))
        {
            return FFbxTransformUtils::TransformNormalByMatrix(LocalNormal, ImportSpace.NormalTransform);
        }

        return Triangle.FallbackNormal;
    }

    FVector4 ReadCornerTangent(
        FbxMesh*                   Mesh,
        int32                      ControlPointIndex,
        int32                      PolygonVertexIndex,
        const FFbxTriangleSample&  Triangle,
        const FFbxMeshImportSpace& ImportSpace,
        const FVector&             ImportedNormal
        ) const
    {
        FVector4 ImportedTangent;
        if (FFbxGeometryReader::TryReadTangent(Mesh, ControlPointIndex, PolygonVertexIndex, ImportedTangent))
        {
            const FVector Tangent = FFbxTransformUtils::TransformTangentByMatrix(
                FVector(ImportedTangent.X, ImportedTangent.Y, ImportedTangent.Z),
                ImportSpace.VertexTransform,
                ImportedNormal
            );

            return FVector4(Tangent.X, Tangent.Y, Tangent.Z, ImportedTangent.W);
        }

        const FVector Tangent = FFbxTransformUtils::OrthogonalizeTangentToNormal(Triangle.FallbackTangent, ImportedNormal);
        return FVector4(Tangent.X, Tangent.Y, Tangent.Z, 1.0f);
    }

    // Finalize 함수의 FBX import 내부 처리 단계를 수행한다.
    bool Finalize()
    {
        SectionBuilder.BuildFinalSections(Materials, MeshAsset.Indices, MeshAsset.Sections);

        if (MeshAsset.Vertices.empty() || MeshAsset.Indices.empty() || MeshAsset.Sections.empty())
        {
            return false;
        }

        MeshAsset.CacheBounds();
        return true;
    }

    void AppendCollisionNode(FbxNode* MeshNode)
    {
        AppendStaticCollisionNode(MeshAsset, MeshNode);
    }
};

// FBX scene의 mesh node들을 하나의 static mesh asset으로 import한다.
bool FFbxStaticMeshImporter::Import(
    FbxScene*                Scene,
    const FString&           SourcePath,
    FStaticMesh&             OutMesh,
    TArray<FStaticMaterial>& OutMaterials,
    FFbxImportContext&       BuildContext
    )
{
    if (!Scene || !Scene->GetRootNode())
    {
        return false;
    }

    TArray<FbxNode*> MeshNodes;
    FFbxSceneQuery::CollectMeshNodes(Scene->GetRootNode(), MeshNodes);
    if (MeshNodes.empty())
    {
        return false;
    }

    TMap<int32, TArray<FbxNode*>> MeshNodesByLOD;
    TArray<FbxNode*>              CollisionNodes;

    for (FbxNode* MeshNode : MeshNodes)
    {
        if (FFbxSceneQuery::IsCollisionProxyNode(MeshNode))
        {
            CollisionNodes.push_back(MeshNode);
            continue;
        }

        const int32 LODIndex = FFbxSceneQuery::GetMeshLODIndex(MeshNode);
        MeshNodesByLOD[LODIndex].push_back(MeshNode);
    }

    if (MeshNodesByLOD.empty())
    {
        return false;
    }

    TArray<int32> SortedLODIndices;
    SortedLODIndices.reserve(MeshNodesByLOD.size());
    for (const auto& Pair : MeshNodesByLOD)
    {
        SortedLODIndices.push_back(Pair.first);
    }
    std::sort(SortedLODIndices.begin(), SortedLODIndices.end());

    const int32 BaseLODIndex = SortedLODIndices[0];
    if (!ImportMeshNodes(MeshNodesByLOD[BaseLODIndex], SourcePath, OutMesh, OutMaterials, BuildContext))
    {
        return false;
    }

    OutMesh.LODModels.clear();
    float BaseScreenSize        = 1.0f;
    float BaseDistanceThreshold = 0.0f;
    if (!MeshNodesByLOD[BaseLODIndex].empty())
    {
        FFbxSceneQuery::TryGetLODSettings(MeshNodesByLOD[BaseLODIndex][0], BaseScreenSize, BaseDistanceThreshold);
    }
    OutMesh.LODModels.push_back(MakeLODFromMesh(OutMesh, BaseLODIndex, BaseScreenSize, BaseDistanceThreshold));

    for (size_t SortedIndex = 1; SortedIndex < SortedLODIndices.size(); ++SortedIndex)
    {
        const int32 LODIndex = SortedLODIndices[SortedIndex];

        FStaticMesh LODMesh;
        if (ImportMeshNodes(MeshNodesByLOD[LODIndex], SourcePath, LODMesh, OutMaterials, BuildContext))
        {
            float ScreenSize        = 1.0f;
            float DistanceThreshold = 0.0f;
            if (!MeshNodesByLOD[LODIndex].empty())
            {
                FFbxSceneQuery::TryGetLODSettings(MeshNodesByLOD[LODIndex][0], ScreenSize, DistanceThreshold);
            }
            OutMesh.LODModels.push_back(MakeLODFromMesh(LODMesh, LODIndex, ScreenSize, DistanceThreshold));
        }
    }

    OutMesh.CollisionShapes.clear();
    for (FbxNode* CollisionNode : CollisionNodes)
    {
        AppendStaticCollisionNode(OutMesh, CollisionNode);
    }

    FFbxMetadataImporter::CollectSceneNodeMetadata(Scene, OutMesh.NodeMetadata);
    FFbxSceneHierarchyImporter::CollectSceneNodes(Scene, OutMesh.SceneNodes);

    return true;
}

bool FFbxStaticMeshImporter::ImportMeshNodes(
    const TArray<FbxNode*>&  MeshNodes,
    const FString&           SourcePath,
    FStaticMesh&             OutMesh,
    TArray<FStaticMaterial>& OutMaterials,
    FFbxImportContext&       BuildContext
    )
{
    if (MeshNodes.empty())
    {
        return false;
    }

    FFbxStaticMeshBuilder Builder(SourcePath, OutMaterials, OutMesh, BuildContext, true);
    return Builder.Build(MeshNodes);
}

bool FFbxStaticMeshImporter::ImportMeshNodesLocal(
    const TArray<FbxNode*>&  MeshNodes,
    const FString&           SourcePath,
    FStaticMesh&             OutMesh,
    TArray<FStaticMaterial>& OutMaterials,
    FFbxImportContext&       BuildContext
    )
{
    if (MeshNodes.empty())
    {
        return false;
    }

    FFbxStaticMeshBuilder Builder(SourcePath, OutMaterials, OutMesh, BuildContext, false);
    return Builder.Build(MeshNodes);
}
