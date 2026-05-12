#include "FbxCollisionImporter.h"

#include "FbxGeometryReader.h"

#include <algorithm>

namespace
{
    static bool StartsWith(const FString& Value, const char* Prefix)
    {
        return Prefix && Value.rfind(Prefix, 0) == 0;
    }

    static void ComputeBounds(const TArray<FVector>& Vertices, FVector& OutMin, FVector& OutMax)
    {
        if (Vertices.empty())
        {
            OutMin = FVector(0.0f, 0.0f, 0.0f);
            OutMax = FVector(0.0f, 0.0f, 0.0f);
            return;
        }

        OutMin = Vertices[0];
        OutMax = Vertices[0];
        for (const FVector& V : Vertices)
        {
            OutMin.X = (std::min)(OutMin.X, V.X);
            OutMin.Y = (std::min)(OutMin.Y, V.Y);
            OutMin.Z = (std::min)(OutMin.Z, V.Z);

            OutMax.X = (std::max)(OutMax.X, V.X);
            OutMax.Y = (std::max)(OutMax.Y, V.Y);
            OutMax.Z = (std::max)(OutMax.Z, V.Z);
        }
    }
}

bool FFbxCollisionImporter::ImportCollisionShape(
    FbxNode*                 MeshNode,
    const FMatrix&           LocalMatrix,
    int32                    ParentBoneIndex,
    const FString&           ParentBoneName,
    FImportedCollisionShape& OutShape
    )
{
    FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;
    if (!Mesh)
    {
        return false;
    }

    OutShape                 = FImportedCollisionShape();
    OutShape.SourceNodeName  = MeshNode->GetName();
    OutShape.Type            = DetectCollisionType(OutShape.SourceNodeName);
    OutShape.LocalMatrix     = LocalMatrix;
    OutShape.ParentBoneIndex = ParentBoneIndex;
    OutShape.ParentBoneName  = ParentBoneName;

    const int32 ControlPointCount = Mesh->GetControlPointsCount();
    OutShape.Vertices.reserve(ControlPointCount);

    // Vertices는 collision mesh local space 그대로 저장한다.
    // LocalMatrix를 별도로 보존해야 rotated box/capsule fitting과 runtime placement가 중복 변환 없이 안정적이다.
    for (int32 ControlPointIndex = 0; ControlPointIndex < ControlPointCount; ++ControlPointIndex)
    {
        OutShape.Vertices.push_back(FFbxGeometryReader::ReadPosition(Mesh, ControlPointIndex));
    }

    for (int32 PolygonIndex = 0; PolygonIndex < Mesh->GetPolygonCount(); ++PolygonIndex)
    {
        const int32 PolygonSize = Mesh->GetPolygonSize(PolygonIndex);

        if (PolygonSize != 3)
        {
            continue;
        }

        for (int32 CornerIndex = 0; CornerIndex < 3; ++CornerIndex)
        {
            const int32 ControlPointIndex = Mesh->GetPolygonVertex(PolygonIndex, CornerIndex);
            if (ControlPointIndex >= 0 && ControlPointIndex < ControlPointCount)
            {
                OutShape.Indices.push_back(static_cast<uint32>(ControlPointIndex));
            }
        }
    }

    if (OutShape.Vertices.empty())
    {
        return false;
    }

    FVector Min, Max;
    ComputeBounds(OutShape.Vertices, Min, Max);

    const FVector Center = (Min + Max) * 0.5f;
    const FVector Extent = (Max - Min) * 0.5f;

    OutShape.LocalCenter = Center;

    if (OutShape.Type == EImportedCollisionType::Box)
    {
        OutShape.BoxExtent = Extent;
    }
    else if (OutShape.Type == EImportedCollisionType::Sphere)
    {
        OutShape.SphereRadius = (std::max)((std::max)(Extent.X, Extent.Y), Extent.Z);
    }
    else if (OutShape.Type == EImportedCollisionType::Capsule)
    {
        OutShape.CapsuleRadius     = (std::max)(Extent.X, Extent.Y);
        OutShape.CapsuleHalfHeight = Extent.Z;
    }

    return true;
}

EImportedCollisionType FFbxCollisionImporter::DetectCollisionType(const FString& NodeName)
{
    if (StartsWith(NodeName, "UBX_"))
    {
        return EImportedCollisionType::Box;
    }

    if (StartsWith(NodeName, "USP_"))
    {
        return EImportedCollisionType::Sphere;
    }

    if (StartsWith(NodeName, "UCP_"))
    {
        return EImportedCollisionType::Capsule;
    }

    if (StartsWith(NodeName, "UCX_") || StartsWith(NodeName, "MCDCX_"))
    {
        return EImportedCollisionType::Convex;
    }

    return EImportedCollisionType::Convex;
}
