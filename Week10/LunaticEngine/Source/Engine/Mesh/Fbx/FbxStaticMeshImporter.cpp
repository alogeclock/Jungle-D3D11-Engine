#include "Mesh/Fbx/FbxStaticMeshImporter.h"

#include "Mesh/Fbx/FbxGeometryReader.h"
#include "Mesh/Fbx/FbxImportContext.h"
#include "Mesh/Fbx/FbxMaterialImporter.h"
#include "Mesh/Fbx/FbxSceneQuery.h"
#include "Mesh/Fbx/FbxSectionBuilder.h"
#include "Mesh/Fbx/FbxTransformUtils.h"
#include "Mesh/Fbx/FbxVertexDeduplicator.h"
#include "Mesh/StaticMeshAsset.h"

#include <fbxsdk.h>

class FFbxStaticMeshBuilder
{
public:
    FFbxStaticMeshBuilder(const FString& InSourcePath, TArray<FStaticMaterial>& InOutMaterials, FStaticMesh& InOutMesh, FFbxImportContext& InBuildContext)
        : SourcePath(InSourcePath), Materials(InOutMaterials), MeshAsset(InOutMesh), BuildContext(InBuildContext)
    {}

    // Build 함수의 FBX import 내부 처리 단계를 수행한다.
    bool Build(const TArray<FbxNode*>& MeshNodes)
    {
        ResetOutput();

        for (FbxNode* MeshNode : MeshNodes)
        {
            AppendMeshNode(MeshNode);
        }

        return Finalize();
    }

private:
    const FString&           SourcePath;
    TArray<FStaticMaterial>& Materials;
    FStaticMesh&             MeshAsset;
    FFbxImportContext&       BuildContext;

    FFbxSectionBuilder           SectionBuilder;
    FFbxStaticVertexDeduplicator VertexDeduplicator;

    // ResetOutput 함수의 FBX import 내부 처리 단계를 수행한다.
    void ResetOutput()
    {
        MeshAsset.Vertices.clear();
        MeshAsset.Indices.clear();
        MeshAsset.Sections.clear();
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

        const FFbxMeshImportSpace ImportSpace = FFbxMeshImportSpace::FromStaticMeshNode(MeshNode);

        int32 PolygonVertexIndex = 0;

        for (int32 PolygonIndex = 0; PolygonIndex < Mesh->GetPolygonCount(); ++PolygonIndex)
        {
            const int32 PolygonSize = Mesh->GetPolygonSize(PolygonIndex);

            if (PolygonSize == 3)
            {
                AppendTriangle(MeshNode, Mesh, PolygonIndex, PolygonVertexIndex, ImportSpace);
            }

            PolygonVertexIndex += PolygonSize;
        }
    }

    // AppendTriangle 함수의 FBX import 내부 처리 단계를 수행한다.
    void AppendTriangle(FbxNode* MeshNode, FbxMesh* Mesh, int32 PolygonIndex, int32 PolygonVertexStartIndex, const FFbxMeshImportSpace& ImportSpace)
    {
        FFbxTriangleSample Triangle;
        if (!FFbxGeometryReader::ReadTriangleSample(Mesh, PolygonIndex, ImportSpace, Triangle))
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
            const uint32 VertexIndex = AddCornerVertex(Mesh, PolygonIndex, CornerIndex, PolygonVertexStartIndex + CornerIndex, Triangle, ImportSpace);

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
        const FFbxMeshImportSpace& ImportSpace
        )
    {
        const int32 ControlPointIndex = Triangle.ControlPointIndices[CornerIndex];

        FNormalVertex Vertex;
        Vertex.pos     = Triangle.Positions[CornerIndex];
        Vertex.normal  = ReadCornerNormal(Mesh, PolygonIndex, CornerIndex, Triangle, ImportSpace);
        Vertex.tex     = Triangle.UV0[CornerIndex];
        Vertex.color   = FFbxGeometryReader::ReadVertexColor(Mesh, ControlPointIndex, PolygonVertexIndex);
        Vertex.tangent = ReadCornerTangent(Mesh, ControlPointIndex, PolygonVertexIndex, Triangle, ImportSpace, Vertex.normal);

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

    FFbxStaticMeshBuilder Builder(SourcePath, OutMaterials, OutMesh, BuildContext);
    return Builder.Build(MeshNodes);
}
