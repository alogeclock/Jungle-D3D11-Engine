#pragma once

#include "Core/CoreTypes.h"

#include <fbxsdk.h>
#include "Mesh/StaticMeshAsset.h"

struct FFbxImportedSectionBuild
{
    int32          MaterialIndex = 0;
    TArray<uint32> Indices;
};

class FFbxSectionBuilder
{
public:
    // material index에 해당하는 임시 section을 찾거나 새로 추가한다.
    FFbxImportedSectionBuild* FindOrAddSection(int32 MaterialIndex);

    // 임시 section 배열을 최종 index buffer와 section 배열로 변환한다.
    void BuildFinalSections(const TArray<FStaticMaterial>& Materials, TArray<uint32>& OutIndices, TArray<FStaticMeshSection>& OutSections) const;

    // 임시 section이 비어 있는지 확인한다.
    bool IsEmpty() const;

private:
    TArray<FFbxImportedSectionBuild> Sections;
};

class FFbxMaterialSlotResolver
{
public:
    // polygon index에 대응하는 FBX material layer index를 읽는다.
    static int32 GetPolygonMaterialIndex(FbxMesh* Mesh, int32 PolygonIndex);

    // polygon index에 대응하는 FbxSurfaceMaterial 포인터를 resolve한다.
    static FbxSurfaceMaterial* ResolvePolygonMaterial(FbxNode* MeshNode, FbxMesh* Mesh, int32 PolygonIndex);
};
