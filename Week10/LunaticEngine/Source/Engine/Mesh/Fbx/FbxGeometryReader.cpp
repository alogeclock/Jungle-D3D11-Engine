#include "Mesh/Fbx/FbxGeometryReader.h"

#include "Mesh/Fbx/FbxTransformUtils.h"

#include <fbxsdk.h>

#include <cmath>

namespace
{
    // FVector의 각 성분이 지정 허용오차 이하인지 검사한다.
    bool IsNearlyZeroVector(const FVector& V, float Tolerance = 1.0e-6f)
    {
        return std::fabs(V.X) <= Tolerance && std::fabs(V.Y) <= Tolerance && std::fabs(V.Z) <= Tolerance;
    }

    const char* GetPrimaryUVSetName(FbxMesh* Mesh, FbxStringList& OutUVSetNames)
    {
        if (!Mesh)
        {
            return nullptr;
        }

        Mesh->GetUVSetNames(OutUVSetNames);

        if (OutUVSetNames.GetCount() <= 0)
        {
            return nullptr;
        }

        return OutUVSetNames[0];
    }

    FVector2 ReadUV(FbxMesh* Mesh, int32 PolygonIndex, int32 CornerIndex)
    {
        if (!Mesh)
        {
            return FVector2(0.0f, 0.0f);
        }

        FbxStringList UVSetNames;
        const char*   UVSetName = GetPrimaryUVSetName(Mesh, UVSetNames);

        if (!UVSetName)
        {
            return FVector2(0.0f, 0.0f);
        }

        FbxVector2 UV;
        bool       bUnmapped = false;

        if (Mesh->GetPolygonVertexUV(PolygonIndex, CornerIndex, UVSetName, UV, bUnmapped))
        {
            return FVector2(static_cast<float>(UV[0]), 1.0f - static_cast<float>(UV[1]));
        }

        return FVector2(0.0f, 0.0f);
    }

    template <typename LayerElementType>
    bool TryGetLayerElementVector4(LayerElementType* Element, int32 ControlPointIndex, int32 PolygonVertexIndex, FbxVector4& OutValue)
    {
        if (!Element)
        {
            return false;
        }

        int32 ElementIndex = 0;

        switch (Element->GetMappingMode())
        {
        case FbxLayerElement::eByControlPoint:
            ElementIndex = ControlPointIndex;
            break;

        case FbxLayerElement::eByPolygonVertex:
            ElementIndex = PolygonVertexIndex;
            break;

        case FbxLayerElement::eAllSame:
            ElementIndex = 0;
            break;

        default:
            return false;
        }

        if (ElementIndex < 0)
        {
            return false;
        }

        if (Element->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
        {
            if (ElementIndex >= Element->GetIndexArray().GetCount())
            {
                return false;
            }

            ElementIndex = Element->GetIndexArray().GetAt(ElementIndex);
        }

        if (ElementIndex < 0 || ElementIndex >= Element->GetDirectArray().GetCount())
        {
            return false;
        }

        OutValue = Element->GetDirectArray().GetAt(ElementIndex);
        return true;
    }
}

// static mesh node의 geometric/global transform으로 import space를 만든다.
FFbxMeshImportSpace FFbxMeshImportSpace::FromStaticMeshNode(FbxNode* MeshNode)
{
    FFbxMeshImportSpace Result;

    if (!MeshNode)
    {
        return Result;
    }

    const FMatrix GeometryTransform = FFbxTransformUtils::ToEngineMatrix(FFbxTransformUtils::GetNodeGeometryTransform(MeshNode));
    const FMatrix GlobalTransform   = FFbxTransformUtils::ToEngineMatrix(MeshNode->EvaluateGlobalTransform());

    Result.VertexTransform = GeometryTransform * GlobalTransform;
    Result.NormalTransform = Result.VertexTransform.GetInverse().GetTransposed();

    return Result;
}

FVector FFbxGeometryReader::ReadPosition(FbxMesh* Mesh, int32 ControlPointIndex)
{
    if (!Mesh) return FVector(0.0f, 0.0f, 0.0f);

    const int32 ControlPointCount = Mesh->GetControlPointsCount();

    if (ControlPointIndex < 0 || ControlPointIndex >= ControlPointCount) return FVector(0.0f, 0.0f, 0.0f);

    FbxVector4*      ControlPoint = Mesh->GetControlPoints();
    const FbxVector4 P            = ControlPoint[ControlPointIndex];

    return FVector(static_cast<float>(P[0]), static_cast<float>(P[1]), static_cast<float>(P[2]));
}

bool FFbxGeometryReader::TryReadNormal(FbxMesh* Mesh, int32 PolygonIndex, int32 CornerIndex, FVector& OutNormal)
{
    FbxVector4 Normal;

    if (Mesh && Mesh->GetPolygonVertexNormal(PolygonIndex, CornerIndex, Normal))
    {
        OutNormal = FVector(static_cast<float>(Normal[0]), static_cast<float>(Normal[1]), static_cast<float>(Normal[2])).Normalized();
        return true;
    }

    OutNormal = FVector(0.0f, 0.0f, 1.0f);
    return false;
}

FVector FFbxGeometryReader::ComputeTriangleNormal(const FVector& P0, const FVector& P1, const FVector& P2)
{
    const FVector E0 = P1 - P0;
    const FVector E1 = P2 - P0;
    const FVector N  = E0.Cross(E1);
    if (N.IsNearlyZero(1.0e-6f))
    {
        return FVector(0.0f, 0.0f, 1.0f);
    }
    return N.Normalized();
}

int32 FFbxGeometryReader::GetUVSetCount(FbxMesh* Mesh)
{
    if (!Mesh)
    {
        return 0;
    }

    FbxStringList UVSetNames;
    Mesh->GetUVSetNames(UVSetNames);
    return static_cast<int32>(UVSetNames.GetCount());
}

FVector2 FFbxGeometryReader::ReadUVByChannel(FbxMesh* Mesh, int32 PolygonIndex, int32 CornerIndex, int32 ChannelIndex)
{
    if (!Mesh)
    {
        return FVector2(0.0f, 0.0f);
    }

    FbxStringList UVSetNames;
    Mesh->GetUVSetNames(UVSetNames);

    if (ChannelIndex < 0 || ChannelIndex >= UVSetNames.GetCount())
    {
        return FVector2(0.0f, 0.0f);
    }

    FbxVector2 UV;
    bool       bUnmapped = false;

    if (Mesh->GetPolygonVertexUV(PolygonIndex, CornerIndex, UVSetNames[ChannelIndex], UV, bUnmapped) && !bUnmapped)
    {
        return FVector2(static_cast<float>(UV[0]), 1.0f - static_cast<float>(UV[1]));
    }

    return FVector2(0.0f, 0.0f);
}

FVector4 FFbxGeometryReader::ReadVertexColor(FbxMesh* Mesh, int32 ControlPointIndex, int32 PolygonVertexIndex)
{
    if (!Mesh || !Mesh->GetLayer(0))
    {
        return FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    FbxLayerElementVertexColor* ColorElement = Mesh->GetLayer(0)->GetVertexColors();
    if (!ColorElement)
    {
        return FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    int32 ColorIndex = 0;

    switch (ColorElement->GetMappingMode())
    {
    case FbxLayerElement::eByControlPoint:
        ColorIndex = ControlPointIndex;
        break;

    case FbxLayerElement::eByPolygonVertex:
        ColorIndex = PolygonVertexIndex;
        break;

    case FbxLayerElement::eAllSame:
        ColorIndex = 0;
        break;

    default:
        return FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    if (ColorIndex < 0)
    {
        return FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    if (ColorElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
    {
        if (ColorIndex >= ColorElement->GetIndexArray().GetCount())
        {
            return FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        }

        ColorIndex = ColorElement->GetIndexArray().GetAt(ColorIndex);
    }

    if (ColorIndex < 0 || ColorIndex >= ColorElement->GetDirectArray().GetCount())
    {
        return FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    const FbxColor C = ColorElement->GetDirectArray().GetAt(ColorIndex);

    return FVector4(static_cast<float>(C.mRed), static_cast<float>(C.mGreen), static_cast<float>(C.mBlue), static_cast<float>(C.mAlpha));
}

bool FFbxGeometryReader::TryReadTangent(FbxMesh* Mesh, int32 ControlPointIndex, int32 PolygonVertexIndex, FVector4& OutTangent)
{
    if (!Mesh || !Mesh->GetLayer(0))
    {
        return false;
    }

    FbxLayerElementTangent* TangentElement = Mesh->GetLayer(0)->GetTangents();
    if (!TangentElement)
    {
        return false;
    }

    int32 TangentIndex = 0;

    switch (TangentElement->GetMappingMode())
    {
    case FbxLayerElement::eByControlPoint:
        TangentIndex = ControlPointIndex;
        break;

    case FbxLayerElement::eByPolygonVertex:
        TangentIndex = PolygonVertexIndex;
        break;

    case FbxLayerElement::eAllSame:
        TangentIndex = 0;
        break;

    default:
        return false;
    }

    if (TangentIndex < 0)
    {
        return false;
    }

    if (TangentElement->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
    {
        if (TangentIndex >= TangentElement->GetIndexArray().GetCount())
        {
            return false;
        }

        TangentIndex = TangentElement->GetIndexArray().GetAt(TangentIndex);
    }

    if (TangentIndex < 0 || TangentIndex >= TangentElement->GetDirectArray().GetCount())
    {
        return false;
    }

    const FbxVector4 T = TangentElement->GetDirectArray().GetAt(TangentIndex);

    FVector Tangent(static_cast<float>(T[0]), static_cast<float>(T[1]), static_cast<float>(T[2]));

    Tangent = Tangent.Normalized();

    const float W = (std::fabs(static_cast<float>(T[3])) > 1.0e-6f) ? static_cast<float>(T[3]) : 1.0f;
    OutTangent    = FVector4(Tangent.X, Tangent.Y, Tangent.Z, W);

    return true;
}

FVector FFbxGeometryReader::ComputeTriangleTangent(
    const FVector&  P0,
    const FVector&  P1,
    const FVector&  P2,
    const FVector2& UV0,
    const FVector2& UV1,
    const FVector2& UV2
    )
{
    const FVector Edge1 = P1 - P0;
    const FVector Edge2 = P2 - P0;

    const float DU1 = UV1.X - UV0.X;
    const float DU2 = UV2.X - UV0.X;
    const float DV1 = UV1.Y - UV0.Y;
    const float DV2 = UV2.Y - UV0.Y;

    const float Denom = DU1 * DV2 - DU2 * DV1;

    if (std::fabs(Denom) <= 1e-6f)
    {
        return FVector(1.0f, 0.0f, 0.0f);
    }

    const float R       = 1.0f / Denom;
    FVector     Tangent = (Edge1 * DV2 - Edge2 * DV1) * R;

    return Tangent.Normalized();
}

bool FFbxGeometryReader::ReadTriangleSample(FbxMesh* Mesh, int32 PolygonIndex, const FFbxMeshImportSpace& ImportSpace, FFbxTriangleSample& OutTriangle)
{
    if (!Mesh || Mesh->GetPolygonSize(PolygonIndex) != 3)
    {
        return false;
    }

    for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
    {
        const int32 ControlPointIndex                = Mesh->GetPolygonVertex(PolygonIndex, CornerIndex);
        OutTriangle.ControlPointIndices[CornerIndex] = ControlPointIndex;

        const FVector LocalPosition        = ReadPosition(Mesh, ControlPointIndex);
        OutTriangle.Positions[CornerIndex] = FFbxTransformUtils::TransformPositionByMatrix(LocalPosition, ImportSpace.VertexTransform);
        OutTriangle.UV0[CornerIndex]       = ReadUVByChannel(Mesh, PolygonIndex, CornerIndex, 0);
    }

    OutTriangle.FallbackNormal = ComputeTriangleNormal(OutTriangle.Positions[0], OutTriangle.Positions[1], OutTriangle.Positions[2]);

    OutTriangle.FallbackTangent = ComputeTriangleTangent(
        OutTriangle.Positions[0],
        OutTriangle.Positions[1],
        OutTriangle.Positions[2],
        OutTriangle.UV0[0],
        OutTriangle.UV0[1],
        OutTriangle.UV0[2]
    );

    return true;
}
