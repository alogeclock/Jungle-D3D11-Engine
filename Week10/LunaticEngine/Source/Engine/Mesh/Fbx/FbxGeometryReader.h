#pragma once

#include "Core/CoreTypes.h"

#include <fbxsdk.h>
#include "Math/Matrix.h"
#include "Math/Vector.h"

struct FFbxMeshImportSpace
{
    FMatrix VertexTransform = FMatrix::Identity;
    FMatrix NormalTransform = FMatrix::Identity;

    // static mesh node의 geometric/global transform으로 import space를 만든다.
    static FFbxMeshImportSpace FromStaticMeshNode(FbxNode* MeshNode);
};

struct FFbxTriangleSample
{
    int32    ControlPointIndices[3] = { -1, -1, -1 };
    FVector  Positions[3];
    FVector2 UV0[3];
    FVector  FallbackNormal;
    FVector  FallbackTangent;
};

class FFbxGeometryReader
{
public:
    // control point 위치를 FBX mesh에서 읽어 엔진 FVector로 반환한다.
    static FVector ReadPosition(FbxMesh* Mesh, int32 ControlPointIndex);

    // polygon corner normal을 FBX layer element mapping/reference mode에 맞춰 읽는다.
    static bool TryReadNormal(FbxMesh* Mesh, int32 PolygonIndex, int32 CornerIndex, FVector& OutNormal);

    // 세 점으로부터 fallback face normal을 계산한다.
    static FVector ComputeTriangleNormal(const FVector& P0, const FVector& P1, const FVector& P2);

    // mesh의 UV set 개수를 반환한다.
    static int32 GetUVSetCount(FbxMesh* Mesh);

    // 지정 UV channel에서 polygon corner UV를 읽는다.
    static FVector2 ReadUVByChannel(FbxMesh* Mesh, int32 PolygonIndex, int32 CornerIndex, int32 ChannelIndex);

    // control point 또는 polygon vertex 기준 vertex color를 읽는다.
    static FVector4 ReadVertexColor(FbxMesh* Mesh, int32 ControlPointIndex, int32 PolygonVertexIndex);

    // control point 또는 polygon vertex 기준 tangent를 읽는다.
    static bool TryReadTangent(FbxMesh* Mesh, int32 ControlPointIndex, int32 PolygonVertexIndex, FVector4& OutTangent);

    // triangle position/UV로 fallback tangent를 계산한다.
    static FVector ComputeTriangleTangent(
        const FVector&  P0,
        const FVector&  P1,
        const FVector&  P2,
        const FVector2& UV0,
        const FVector2& UV1,
        const FVector2& UV2
        );

    // triangulated polygon 하나의 control point, position, UV, fallback normal/tangent를 샘플링한다.
    static bool ReadTriangleSample(FbxMesh* Mesh, int32 PolygonIndex, const FFbxMeshImportSpace& ImportSpace, FFbxTriangleSample& OutTriangle);
};
