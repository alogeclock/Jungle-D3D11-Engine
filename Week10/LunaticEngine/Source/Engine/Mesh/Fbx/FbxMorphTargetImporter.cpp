#include "Mesh/Fbx/FbxMorphTargetImporter.h"

#include "Mesh/Fbx/FbxTransformUtils.h"

#include <fbxsdk.h>

#include <cmath>
#include <utility>

namespace
{
    // FVector의 각 성분이 지정 허용오차 이하인지 검사한다.
    bool IsNearlyZeroVector(const FVector& V, float Tolerance = 1.0e-6f)
    {
        return std::fabs(V.X) <= Tolerance && std::fabs(V.Y) <= Tolerance && std::fabs(V.Z) <= Tolerance;
    }

    // FVector4의 각 성분이 지정 허용오차 이하인지 검사한다.
    bool IsNearlyZeroVector4(const FVector4& V, float Tolerance = 1.0e-6f)
    {
        return std::fabs(V.X) <= Tolerance && std::fabs(V.Y) <= Tolerance && std::fabs(V.Z) <= Tolerance && std::fabs(V.W) <= Tolerance;
    }

    // FBX layer element에서 control point 또는 polygon vertex 기준 Vector4 값을 읽는다.
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

    static bool TryReadShapeNormal(FbxShape* Shape, int32 ControlPointIndex, int32 PolygonVertexIndex, FVector& OutNormal)
    {
        if (!Shape || !Shape->GetLayer(0))
        {
            return false;
        }

        FbxVector4 Value;
        if (!TryGetLayerElementVector4(Shape->GetLayer(0)->GetNormals(), ControlPointIndex, PolygonVertexIndex, Value))
        {
            return false;
        }

        OutNormal = FVector(static_cast<float>(Value[0]), static_cast<float>(Value[1]), static_cast<float>(Value[2])).Normalized();
        return true;
    }

    static bool TryReadShapeTangent(FbxShape* Shape, int32 ControlPointIndex, int32 PolygonVertexIndex, FVector4& OutTangent)
    {
        if (!Shape || !Shape->GetLayer(0))
        {
            return false;
        }

        FbxVector4 Value;
        if (!TryGetLayerElementVector4(Shape->GetLayer(0)->GetTangents(), ControlPointIndex, PolygonVertexIndex, Value))
        {
            return false;
        }

        FVector Tangent(static_cast<float>(Value[0]), static_cast<float>(Value[1]), static_cast<float>(Value[2]));
        Tangent = Tangent.Normalized();

        const float W = (std::fabs(static_cast<float>(Value[3])) > 1.0e-6f) ? static_cast<float>(Value[3]) : 1.0f;
        OutTangent    = FVector4(Tangent.X, Tangent.Y, Tangent.Z, W);
        return true;
    }
}

void FFbxMorphTargetImporter::ImportMorphTargets(const TArray<TArray<FFbxImportedMorphSourceVertex>>& MorphSourcesByLOD, TArray<FMorphTarget>& OutMorphTargets)
{
    OutMorphTargets.clear();

    TMap<FString, int32> MorphNameToIndex;

    for (int32 LODIndex = 0; LODIndex < static_cast<int32>(MorphSourcesByLOD.size()); ++LODIndex)
    {
        const TArray<FFbxImportedMorphSourceVertex>& Sources = MorphSourcesByLOD[LODIndex];

        TMap<FbxMesh*, TArray<const FFbxImportedMorphSourceVertex*>> SourcesByMesh;

        for (const FFbxImportedMorphSourceVertex& Source : Sources)
        {
            if (Source.Mesh)
            {
                SourcesByMesh[Source.Mesh].push_back(&Source);
            }
        }

        for (const auto& MeshPair : SourcesByMesh)
        {
            FbxMesh*                                            Mesh        = MeshPair.first;
            const TArray<const FFbxImportedMorphSourceVertex*>& MeshSources = MeshPair.second;

            if (!Mesh)
            {
                continue;
            }

            const int32 BlendShapeCount = Mesh->GetDeformerCount(FbxDeformer::eBlendShape);

            for (int32 BlendShapeIndex = 0; BlendShapeIndex < BlendShapeCount; ++BlendShapeIndex)
            {
                FbxBlendShape* BlendShape = static_cast<FbxBlendShape*>(Mesh->GetDeformer(BlendShapeIndex, FbxDeformer::eBlendShape));

                if (!BlendShape)
                {
                    continue;
                }

                const int32 ChannelCount = BlendShape->GetBlendShapeChannelCount();

                for (int32 ChannelIndex = 0; ChannelIndex < ChannelCount; ++ChannelIndex)
                {
                    FbxBlendShapeChannel* Channel = BlendShape->GetBlendShapeChannel(ChannelIndex);

                    if (!Channel)
                    {
                        continue;
                    }

                    const int32 TargetShapeCount = Channel->GetTargetShapeCount();

                    if (TargetShapeCount <= 0)
                    {
                        continue;
                    }

                    FbxShape* Shape = Channel->GetTargetShape(TargetShapeCount - 1);

                    if (!Shape)
                    {
                        continue;
                    }

                    FString MorphName = Channel->GetName();

                    if (MorphName.empty())
                    {
                        MorphName = Shape->GetName();
                    }

                    int32 MorphIndex = -1;

                    auto Existing = MorphNameToIndex.find(MorphName);
                    if (Existing != MorphNameToIndex.end())
                    {
                        MorphIndex = Existing->second;
                    }
                    else
                    {
                        FMorphTarget NewMorph;
                        NewMorph.Name = MorphName;
                        NewMorph.LODModels.resize(MorphSourcesByLOD.size());

                        OutMorphTargets.push_back(std::move(NewMorph));

                        MorphIndex                  = static_cast<int32>(OutMorphTargets.size()) - 1;
                        MorphNameToIndex[MorphName] = MorphIndex;
                    }

                    FMorphTargetLOD& MorphLOD = OutMorphTargets[MorphIndex].LODModels[LODIndex];

                    FbxVector4* BaseControlPoints   = Mesh->GetControlPoints();
                    FbxVector4* TargetControlPoints = Shape->GetControlPoints();

                    if (!BaseControlPoints || !TargetControlPoints)
                    {
                        continue;
                    }

                    const int32 ControlPointCount = Mesh->GetControlPointsCount();

                    for (const FFbxImportedMorphSourceVertex* Source : MeshSources)
                    {
                        if (!Source)
                        {
                            continue;
                        }

                        const int32 CPIndex = Source->ControlPointIndex;

                        if (CPIndex < 0 || CPIndex >= ControlPointCount)
                        {
                            continue;
                        }

                        const FbxVector4 BaseP   = BaseControlPoints[CPIndex];
                        const FbxVector4 TargetP = TargetControlPoints[CPIndex];

                        FVector LocalDelta(
                            static_cast<float>(TargetP[0] - BaseP[0]),
                            static_cast<float>(TargetP[1] - BaseP[1]),
                            static_cast<float>(TargetP[2] - BaseP[2])
                        );

                        FMorphTargetDelta Delta;
                        Delta.VertexIndex   = Source->VertexIndex;
                        Delta.PositionDelta = FFbxTransformUtils::TransformVectorNoNormalizeByMatrix(LocalDelta, Source->MeshToReference);
                        Delta.NormalDelta   = FVector(0.0f, 0.0f, 0.0f);
                        Delta.TangentDelta  = FVector4(0.0f, 0.0f, 0.0f, 0.0f);

                        FVector TargetNormalInReference = Source->BaseNormalInReference;

                        FVector TargetLocalNormal;
                        if (TryReadShapeNormal(Shape, CPIndex, Source->PolygonVertexIndex, TargetLocalNormal))
                        {
                            TargetNormalInReference = FFbxTransformUtils::TransformNormalByMatrix(TargetLocalNormal, Source->NormalToReference);
                            Delta.NormalDelta       = TargetNormalInReference - Source->BaseNormalInReference;
                        }

                        FVector4 TargetLocalTangent;
                        if (TryReadShapeTangent(Shape, CPIndex, Source->PolygonVertexIndex, TargetLocalTangent))
                        {
                            const FVector TargetTangentInReference = FFbxTransformUtils::TransformTangentByMatrix(
                                FVector(TargetLocalTangent.X, TargetLocalTangent.Y, TargetLocalTangent.Z),
                                Source->MeshToReference,
                                TargetNormalInReference
                            );
                            Delta.TangentDelta = FVector4(
                                TargetTangentInReference.X - Source->BaseTangentInReference.X,
                                TargetTangentInReference.Y - Source->BaseTangentInReference.Y,
                                TargetTangentInReference.Z - Source->BaseTangentInReference.Z,
                                TargetLocalTangent.W - Source->BaseTangentInReference.W
                            );
                        }

                        if (IsNearlyZeroVector(Delta.PositionDelta) && IsNearlyZeroVector(Delta.NormalDelta) && IsNearlyZeroVector4(Delta.TangentDelta))
                        {
                            continue;
                        }

                        MorphLOD.Deltas.push_back(Delta);
                    }
                }
            }
        }
    }
}
