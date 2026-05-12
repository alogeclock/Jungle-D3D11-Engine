#include "Mesh/Fbx/FbxAnimationImporter.h"

#include "Mesh/Fbx/FbxSceneQuery.h"
#include "Mesh/Fbx/FbxTransformUtils.h"

#include <fbxsdk.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
    static float GetSceneSampleRate(FbxScene* Scene)
    {
        if (!Scene)
        {
            return 30.0f;
        }

        const FbxTime::EMode TimeMode  = Scene->GetGlobalSettings().GetTimeMode();
        const double         FrameRate = FbxTime::GetFrameRate(TimeMode);

        if (FrameRate > 1.0)
        {
            return static_cast<float>(FrameRate);
        }

        return 30.0f;
    }

    static int32 FindOrAddFloatCurve(
        FSkeletalAnimationClip&  Clip,
        const FString&           CurveName,
        EAnimationFloatCurveType CurveType,
        const FString&           TargetName,
        const FString&           SourceMeshNodeName,
        const FString&           SourceBlendShapeName,
        const FString&           SourceChannelName,
        int32                    LayerIndex,
        const FString&           LayerName
        )
    {
        for (int32 CurveIndex = 0; CurveIndex < static_cast<int32>(Clip.FloatCurves.size()); ++CurveIndex)
        {
            const FAnimationFloatCurve& Existing = Clip.FloatCurves[CurveIndex];

            if (Existing.Name == CurveName && Existing.Type == CurveType && Existing.TargetName == TargetName && Existing.SourceMeshNodeName ==
                SourceMeshNodeName && Existing.SourceBlendShapeName == SourceBlendShapeName && Existing.SourceChannelName == SourceChannelName && Existing.
                LayerIndex == LayerIndex && Existing.LayerName == LayerName)
            {
                return CurveIndex;
            }
        }

        FAnimationFloatCurve NewCurve;

        NewCurve.Name                 = CurveName;
        NewCurve.Type                 = CurveType;
        NewCurve.TargetName           = TargetName;
        NewCurve.SourceMeshNodeName   = SourceMeshNodeName;
        NewCurve.SourceBlendShapeName = SourceBlendShapeName;
        NewCurve.SourceChannelName    = SourceChannelName;
        NewCurve.LayerIndex           = LayerIndex;
        NewCurve.LayerName            = LayerName;

        Clip.FloatCurves.push_back(std::move(NewCurve));

        return static_cast<int32>(Clip.FloatCurves.size()) - 1;
    }

    static EAnimationCurveInterpolation ConvertFbxInterpolation(FbxAnimCurveDef::EInterpolationType Interpolation)
    {
        switch (Interpolation)
        {
        case FbxAnimCurveDef::eInterpolationConstant:
            return EAnimationCurveInterpolation::Constant;
        case FbxAnimCurveDef::eInterpolationLinear:
            return EAnimationCurveInterpolation::Linear;
        case FbxAnimCurveDef::eInterpolationCubic:
            return EAnimationCurveInterpolation::Cubic;
        default:
            return EAnimationCurveInterpolation::Unknown;
        }
    }

    static FFloatCurveKey MakeFloatCurveKey(FbxAnimCurve* Curve, int32 KeyIndex, double StartSeconds)
    {
        FFloatCurveKey Key;

        if (!Curve)
        {
            return Key;
        }

        const double AbsoluteSeconds = Curve->KeyGetTime(KeyIndex).GetSecondDouble();

        Key.TimeSeconds = static_cast<float>(AbsoluteSeconds - StartSeconds);
        Key.Value       = static_cast<float>(Curve->KeyGetValue(KeyIndex));

        Key.Interpolation = ConvertFbxInterpolation(Curve->KeyGetInterpolation(KeyIndex));

        Key.LeftDerivative  = Curve->KeyGetLeftDerivative(KeyIndex);
        Key.RightDerivative = Curve->KeyGetRightDerivative(KeyIndex);

        Key.bLeftTangentWeighted  = Curve->KeyIsLeftTangentWeighted(KeyIndex);
        Key.bRightTangentWeighted = Curve->KeyIsRightTangentWeighted(KeyIndex);

        Key.LeftTangentWeight = Key.bLeftTangentWeighted ? Curve->KeyGetLeftTangentWeight(KeyIndex) : 0.0f;

        Key.RightTangentWeight = Key.bRightTangentWeighted ? Curve->KeyGetRightTangentWeight(KeyIndex) : 0.0f;

        return Key;
    }

    static void AppendCurveKeysInRange(FbxAnimCurve* Curve, double StartSeconds, double EndSeconds, TArray<FFloatCurveKey>& OutKeys)
    {
        if (!Curve)
        {
            return;
        }

        const int32 KeyCount = Curve->KeyGetCount();

        for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
        {
            const double AbsoluteSeconds = Curve->KeyGetTime(KeyIndex).GetSecondDouble();

            if (AbsoluteSeconds < StartSeconds - 1.0e-6 || AbsoluteSeconds > EndSeconds + 1.0e-6)
            {
                continue;
            }

            OutKeys.push_back(MakeFloatCurveKey(Curve, KeyIndex, StartSeconds));
        }
    }

    static bool TryUseTimeSpan(const FbxTimeSpan& TimeSpan, double& OutStartSeconds, double& OutEndSeconds)
    {
        const double StartSeconds = TimeSpan.GetStart().GetSecondDouble();
        const double EndSeconds   = TimeSpan.GetStop().GetSecondDouble();

        if (EndSeconds <= StartSeconds)
        {
            return false;
        }

        OutStartSeconds = StartSeconds;
        OutEndSeconds   = EndSeconds;

        return true;
    }

    static void UpdateTimeRangeFromCurve(FbxAnimCurve* Curve, double& InOutStartSeconds, double& InOutEndSeconds, bool& bHasAnyKey)
    {
        if (!Curve)
        {
            return;
        }

        const int32 KeyCount = Curve->KeyGetCount();

        for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
        {
            const double KeySeconds = Curve->KeyGetTime(KeyIndex).GetSecondDouble();

            if (!bHasAnyKey)
            {
                InOutStartSeconds = KeySeconds;
                InOutEndSeconds   = KeySeconds;
                bHasAnyKey        = true;
            }
            else
            {
                InOutStartSeconds = (std::min)(InOutStartSeconds, KeySeconds);
                InOutEndSeconds   = (std::max)(InOutEndSeconds, KeySeconds);
            }
        }
    }

    static void UpdateTimeRangeFromBoneCurves(
        FbxAnimLayer*                AnimLayer,
        const TMap<FbxNode*, int32>& BoneNodeToIndex,
        double&                      InOutStartSeconds,
        double&                      InOutEndSeconds,
        bool&                        bHasAnyKey
        )
    {
        if (!AnimLayer)
        {
            return;
        }

        for (const auto& Pair : BoneNodeToIndex)
        {
            FbxNode* BoneNode = Pair.first;

            if (!BoneNode)
            {
                continue;
            }

            UpdateTimeRangeFromCurve(
                BoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X),
                InOutStartSeconds,
                InOutEndSeconds,
                bHasAnyKey
            );
            UpdateTimeRangeFromCurve(
                BoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y),
                InOutStartSeconds,
                InOutEndSeconds,
                bHasAnyKey
            );
            UpdateTimeRangeFromCurve(
                BoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z),
                InOutStartSeconds,
                InOutEndSeconds,
                bHasAnyKey
            );

            UpdateTimeRangeFromCurve(BoneNode->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X), InOutStartSeconds, InOutEndSeconds, bHasAnyKey);
            UpdateTimeRangeFromCurve(BoneNode->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y), InOutStartSeconds, InOutEndSeconds, bHasAnyKey);
            UpdateTimeRangeFromCurve(BoneNode->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z), InOutStartSeconds, InOutEndSeconds, bHasAnyKey);

            UpdateTimeRangeFromCurve(BoneNode->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X), InOutStartSeconds, InOutEndSeconds, bHasAnyKey);
            UpdateTimeRangeFromCurve(BoneNode->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y), InOutStartSeconds, InOutEndSeconds, bHasAnyKey);
            UpdateTimeRangeFromCurve(BoneNode->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z), InOutStartSeconds, InOutEndSeconds, bHasAnyKey);
        }
    }

    static void UpdateTimeRangeFromBlendShapeCurves(
        FbxScene*     Scene,
        FbxAnimLayer* AnimLayer,
        double&       InOutStartSeconds,
        double&       InOutEndSeconds,
        bool&         bHasAnyKey
        )
    {
        if (!Scene || !AnimLayer || !Scene->GetRootNode())
        {
            return;
        }

        TArray<FbxNode*> MeshNodes;
        FFbxSceneQuery::CollectMeshNodes(Scene->GetRootNode(), MeshNodes);

        for (FbxNode* MeshNode : MeshNodes)
        {
            FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;
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

                    UpdateTimeRangeFromCurve(Channel->DeformPercent.GetCurve(AnimLayer), InOutStartSeconds, InOutEndSeconds, bHasAnyKey);
                }
            }
        }
    }

    static bool TryFindAnimationCurveTimeRange(
        FbxScene*                    Scene,
        FbxAnimStack*                AnimStack,
        const TMap<FbxNode*, int32>& BoneNodeToIndex,
        double&                      OutStartSeconds,
        double&                      OutEndSeconds
        )
    {
        if (!AnimStack)
        {
            return false;
        }

        bool   bHasAnyKey   = false;
        double StartSeconds = 0.0;
        double EndSeconds   = 0.0;

        const int32 LayerCount = AnimStack->GetMemberCount<FbxAnimLayer>();

        for (int32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
        {
            FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>(LayerIndex);

            UpdateTimeRangeFromBoneCurves(AnimLayer, BoneNodeToIndex, StartSeconds, EndSeconds, bHasAnyKey);
            UpdateTimeRangeFromBlendShapeCurves(Scene, AnimLayer, StartSeconds, EndSeconds, bHasAnyKey);
        }

        if (!bHasAnyKey || EndSeconds <= StartSeconds)
        {
            return false;
        }

        OutStartSeconds = StartSeconds;
        OutEndSeconds   = EndSeconds;

        return true;
    }

    static bool ResolveAnimationTimeRange(
        FbxScene*                    Scene,
        FbxAnimStack*                AnimStack,
        const TMap<FbxNode*, int32>& BoneNodeToIndex,
        double&                      OutStartSeconds,
        double&                      OutEndSeconds
        )
    {
        if (!Scene || !AnimStack)
        {
            return false;
        }

        if (TryUseTimeSpan(AnimStack->GetLocalTimeSpan(), OutStartSeconds, OutEndSeconds))
        {
            return true;
        }

        if (TryUseTimeSpan(AnimStack->GetReferenceTimeSpan(), OutStartSeconds, OutEndSeconds))
        {
            return true;
        }

        FbxTimeSpan TimelineTimeSpan;
        Scene->GetGlobalSettings().GetTimelineDefaultTimeSpan(TimelineTimeSpan);

        if (TryUseTimeSpan(TimelineTimeSpan, OutStartSeconds, OutEndSeconds))
        {
            return true;
        }

        return TryFindAnimationCurveTimeRange(Scene, AnimStack, BoneNodeToIndex, OutStartSeconds, OutEndSeconds);
    }

    static void ImportBlendShapeCurvesForLayer(
        FbxScene*               Scene,
        FbxAnimLayer*           AnimLayer,
        int32                   LayerIndex,
        double                  StartSeconds,
        double                  EndSeconds,
        FSkeletalAnimationClip& Clip
        )
    {
        if (!Scene || !AnimLayer || !Scene->GetRootNode())
        {
            return;
        }

        TArray<FbxNode*> MeshNodes;
        FFbxSceneQuery::CollectMeshNodes(Scene->GetRootNode(), MeshNodes);

        for (FbxNode* MeshNode : MeshNodes)
        {
            FbxMesh* Mesh = MeshNode ? MeshNode->GetMesh() : nullptr;
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

                    FbxAnimCurve* Curve = Channel->DeformPercent.GetCurve(AnimLayer);
                    if (!Curve || Curve->KeyGetCount() <= 0)
                    {
                        continue;
                    }

                    FString CurveName = Channel->GetName() ? FString(Channel->GetName()) : FString();
                    if (CurveName.empty())
                    {
                        CurveName = MeshNode && MeshNode->GetName() ? FString(MeshNode->GetName()) : FString();
                    }

                    if (CurveName.empty())
                    {
                        continue;
                    }

                    const FString SourceMeshNodeName = MeshNode && MeshNode->GetName() ? FString(MeshNode->GetName()) : FString();

                    const FString SourceBlendShapeName = BlendShape && BlendShape->GetName() ? FString(BlendShape->GetName()) : FString();

                    const FString SourceChannelName = Channel && Channel->GetName() ? FString(Channel->GetName()) : CurveName;

                    const FString LayerName = AnimLayer && AnimLayer->GetName() ? FString(AnimLayer->GetName()) : FString();

                    const int32 CurveIndex = FindOrAddFloatCurve(
                        Clip,
                        CurveName,
                        EAnimationFloatCurveType::MorphTarget,
                        CurveName,
                        SourceMeshNodeName,
                        SourceBlendShapeName,
                        SourceChannelName,
                        LayerIndex,
                        LayerName
                    );

                    FAnimationFloatCurve& OutCurve = Clip.FloatCurves[CurveIndex];
                    AppendCurveKeysInRange(Curve, StartSeconds, EndSeconds, OutCurve.Keys);
                }
            }
        }
    }

    static void ImportBlendShapeCurves(FbxScene* Scene, FbxAnimStack* AnimStack, double StartSeconds, double EndSeconds, FSkeletalAnimationClip& Clip)
    {
        if (!Scene || !AnimStack)
        {
            return;
        }

        const int32 LayerCount = AnimStack->GetMemberCount<FbxAnimLayer>();

        for (int32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
        {
            FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>(LayerIndex);
            ImportBlendShapeCurvesForLayer(Scene, AnimLayer, LayerIndex, StartSeconds, EndSeconds, Clip);
        }
    }

    static FString GetRawCurveTargetSuffix(EBoneRawCurveTarget Target)
    {
        switch (Target)
        {
        case EBoneRawCurveTarget::TranslationX:
            return "TranslationX";
        case EBoneRawCurveTarget::TranslationY:
            return "TranslationY";
        case EBoneRawCurveTarget::TranslationZ:
            return "TranslationZ";

        case EBoneRawCurveTarget::RotationX:
            return "RotationX";
        case EBoneRawCurveTarget::RotationY:
            return "RotationY";
        case EBoneRawCurveTarget::RotationZ:
            return "RotationZ";

        case EBoneRawCurveTarget::ScaleX:
            return "ScaleX";
        case EBoneRawCurveTarget::ScaleY:
            return "ScaleY";
        case EBoneRawCurveTarget::ScaleZ:
            return "ScaleZ";

        default:
            return "Unknown";
        }
    }

    static void ImportSingleBoneRawCurve(
        FbxAnimCurve*        Curve,
        EBoneRawCurveTarget  Target,
        const FString&       BoneName,
        int32                LayerIndex,
        const FString&       LayerName,
        double               StartSeconds,
        double               EndSeconds,
        FBoneAnimationTrack& Track
        )
    {
        if (!Curve || Curve->KeyGetCount() <= 0)
        {
            return;
        }

        FBoneRawFloatCurve RawCurve;
        RawCurve.Target = Target;

        const FString TargetSuffix = GetRawCurveTargetSuffix(Target);

        RawCurve.Curve.Name       = BoneName + FString(".") + TargetSuffix;
        RawCurve.Curve.Type       = EAnimationFloatCurveType::BoneRaw;
        RawCurve.Curve.TargetName = BoneName;
        RawCurve.Curve.LayerIndex = LayerIndex;
        RawCurve.Curve.LayerName  = LayerName;

        AppendCurveKeysInRange(Curve, StartSeconds, EndSeconds, RawCurve.Curve.Keys);

        if (!RawCurve.Curve.Keys.empty())
        {
            Track.RawCurves.push_back(std::move(RawCurve));
        }
    }

    static void ImportBoneRawCurvesForLayer(
        FbxAnimLayer*                AnimLayer,
        int32                        LayerIndex,
        const TMap<FbxNode*, int32>& BoneNodeToIndex,
        const FSkeleton&             Skeleton,
        double                       StartSeconds,
        double                       EndSeconds,
        FSkeletalAnimationClip&      Clip
        )
    {
        if (!AnimLayer)
        {
            return;
        }

        const FString LayerName = AnimLayer->GetName() ? FString(AnimLayer->GetName()) : FString();

        for (const auto& Pair : BoneNodeToIndex)
        {
            FbxNode*    BoneNode  = Pair.first;
            const int32 BoneIndex = Pair.second;

            if (!BoneNode || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Skeleton.Bones.size()))
            {
                continue;
            }

            if (BoneIndex >= static_cast<int32>(Clip.Tracks.size()))
            {
                continue;
            }

            FBoneAnimationTrack& Track    = Clip.Tracks[BoneIndex];
            const FString&       BoneName = Skeleton.Bones[BoneIndex].Name;

            ImportSingleBoneRawCurve(
                BoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X),
                EBoneRawCurveTarget::TranslationX,
                BoneName,
                LayerIndex,
                LayerName,
                StartSeconds,
                EndSeconds,
                Track
            );
            ImportSingleBoneRawCurve(
                BoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y),
                EBoneRawCurveTarget::TranslationY,
                BoneName,
                LayerIndex,
                LayerName,
                StartSeconds,
                EndSeconds,
                Track
            );
            ImportSingleBoneRawCurve(
                BoneNode->LclTranslation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z),
                EBoneRawCurveTarget::TranslationZ,
                BoneName,
                LayerIndex,
                LayerName,
                StartSeconds,
                EndSeconds,
                Track
            );

            ImportSingleBoneRawCurve(
                BoneNode->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X),
                EBoneRawCurveTarget::RotationX,
                BoneName,
                LayerIndex,
                LayerName,
                StartSeconds,
                EndSeconds,
                Track
            );
            ImportSingleBoneRawCurve(
                BoneNode->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y),
                EBoneRawCurveTarget::RotationY,
                BoneName,
                LayerIndex,
                LayerName,
                StartSeconds,
                EndSeconds,
                Track
            );
            ImportSingleBoneRawCurve(
                BoneNode->LclRotation.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z),
                EBoneRawCurveTarget::RotationZ,
                BoneName,
                LayerIndex,
                LayerName,
                StartSeconds,
                EndSeconds,
                Track
            );

            ImportSingleBoneRawCurve(
                BoneNode->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_X),
                EBoneRawCurveTarget::ScaleX,
                BoneName,
                LayerIndex,
                LayerName,
                StartSeconds,
                EndSeconds,
                Track
            );
            ImportSingleBoneRawCurve(
                BoneNode->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Y),
                EBoneRawCurveTarget::ScaleY,
                BoneName,
                LayerIndex,
                LayerName,
                StartSeconds,
                EndSeconds,
                Track
            );
            ImportSingleBoneRawCurve(
                BoneNode->LclScaling.GetCurve(AnimLayer, FBXSDK_CURVENODE_COMPONENT_Z),
                EBoneRawCurveTarget::ScaleZ,
                BoneName,
                LayerIndex,
                LayerName,
                StartSeconds,
                EndSeconds,
                Track
            );
        }
    }

    static void ImportBoneRawCurves(
        FbxAnimStack*                AnimStack,
        const TMap<FbxNode*, int32>& BoneNodeToIndex,
        const FSkeleton&             Skeleton,
        double                       StartSeconds,
        double                       EndSeconds,
        FSkeletalAnimationClip&      Clip
        )
    {
        if (!AnimStack)
        {
            return;
        }

        const int32 LayerCount = AnimStack->GetMemberCount<FbxAnimLayer>();

        for (int32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
        {
            FbxAnimLayer* AnimLayer = AnimStack->GetMember<FbxAnimLayer>(LayerIndex);
            ImportBoneRawCurvesForLayer(AnimLayer, LayerIndex, BoneNodeToIndex, Skeleton, StartSeconds, EndSeconds, Clip);
        }
    }

    static bool LessRawCurve(const FBoneRawFloatCurve& A, const FBoneRawFloatCurve& B)
    {
        if (A.Curve.LayerIndex != B.Curve.LayerIndex)
        {
            return A.Curve.LayerIndex < B.Curve.LayerIndex;
        }

        if (A.Curve.LayerName != B.Curve.LayerName)
        {
            return A.Curve.LayerName < B.Curve.LayerName;
        }

        return static_cast<uint8>(A.Target) < static_cast<uint8>(B.Target);
    }

    static bool LessFloatCurve(const FAnimationFloatCurve& A, const FAnimationFloatCurve& B)
    {
        if (A.Type != B.Type)
        {
            return static_cast<uint8>(A.Type) < static_cast<uint8>(B.Type);
        }

        if (A.LayerIndex != B.LayerIndex)
        {
            return A.LayerIndex < B.LayerIndex;
        }

        if (A.LayerName != B.LayerName)
        {
            return A.LayerName < B.LayerName;
        }

        if (A.SourceMeshNodeName != B.SourceMeshNodeName)
        {
            return A.SourceMeshNodeName < B.SourceMeshNodeName;
        }

        if (A.SourceBlendShapeName != B.SourceBlendShapeName)
        {
            return A.SourceBlendShapeName < B.SourceBlendShapeName;
        }

        if (A.SourceChannelName != B.SourceChannelName)
        {
            return A.SourceChannelName < B.SourceChannelName;
        }

        return A.Name < B.Name;
    }

    static void SortAnimationClipKeys(FSkeletalAnimationClip& Clip)
    {
        for (FBoneAnimationTrack& Track : Clip.Tracks)
        {
            std::sort(
                Track.Keys.begin(),
                Track.Keys.end(),
                [](const FBoneTransformKey& A, const FBoneTransformKey& B)
                {
                    return A.TimeSeconds < B.TimeSeconds;
                }
            );

            std::sort(Track.RawCurves.begin(), Track.RawCurves.end(), LessRawCurve);

            for (FBoneRawFloatCurve& RawCurve : Track.RawCurves)
            {
                std::sort(
                    RawCurve.Curve.Keys.begin(),
                    RawCurve.Curve.Keys.end(),
                    [](const FFloatCurveKey& A, const FFloatCurveKey& B)
                    {
                        return A.TimeSeconds < B.TimeSeconds;
                    }
                );
            }
        }

        std::sort(Clip.FloatCurves.begin(), Clip.FloatCurves.end(), LessFloatCurve);

        for (FAnimationFloatCurve& Curve : Clip.FloatCurves)
        {
            std::sort(
                Curve.Keys.begin(),
                Curve.Keys.end(),
                [](const FFloatCurveKey& A, const FFloatCurveKey& B)
                {
                    return A.TimeSeconds < B.TimeSeconds;
                }
            );
        }
    }
}

void FFbxAnimationImporter::ImportAnimations(
    FbxScene*                       Scene,
    const TMap<FbxNode*, int32>&    BoneNodeToIndex,
    const FMatrix&                  ReferenceMeshBindInverse,
    const FSkeleton&                Skeleton,
    TArray<FSkeletalAnimationClip>& OutAnimations
    )
{
    OutAnimations.clear();

    if (!Scene)
    {
        return;
    }

    const int32 AnimStackCount = Scene->GetSrcObjectCount<FbxAnimStack>();
    if (AnimStackCount <= 0)
    {
        return;
    }

    const float SampleRate = GetSceneSampleRate(Scene);

    for (int32 StackIndex = 0; StackIndex < AnimStackCount; ++StackIndex)
    {
        FbxAnimStack* AnimStack = Scene->GetSrcObject<FbxAnimStack>(StackIndex);
        if (!AnimStack)
        {
            continue;
        }

        Scene->SetCurrentAnimationStack(AnimStack);

        double StartSeconds = 0.0;
        double EndSeconds   = 0.0;

        if (!ResolveAnimationTimeRange(Scene, AnimStack, BoneNodeToIndex, StartSeconds, EndSeconds))
        {
            continue;
        }

        FSkeletalAnimationClip Clip;
        Clip.Name            = AnimStack->GetName() ? FString(AnimStack->GetName()) : FString();
        Clip.DurationSeconds = static_cast<float>(EndSeconds - StartSeconds);
        Clip.SampleRate      = SampleRate;

        Clip.Tracks.resize(Skeleton.Bones.size());

        for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Skeleton.Bones.size()); ++BoneIndex)
        {
            FBoneAnimationTrack& Track = Clip.Tracks[BoneIndex];
            Track.BoneIndex            = BoneIndex;
            Track.BoneName             = Skeleton.Bones[BoneIndex].Name;
        }

        const double DurationSeconds = static_cast<double>(Clip.DurationSeconds);
        const int32  WholeFrameCount = static_cast<int32>(std::floor(DurationSeconds * static_cast<double>(SampleRate) + 1.0e-6));

        auto AddAnimationKeysAtTime = [&](double LocalSeconds)
        {
            if (LocalSeconds < 0.0)
            {
                LocalSeconds = 0.0;
            }
            else if (LocalSeconds > DurationSeconds)
            {
                LocalSeconds = DurationSeconds;
            }

            const double AbsoluteSeconds = StartSeconds + LocalSeconds;

            FbxTime Time;
            Time.SetSecondDouble(AbsoluteSeconds);

            TArray<FMatrix> GlobalInReference;
            GlobalInReference.resize(Skeleton.Bones.size());

            for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Skeleton.Bones.size()); ++BoneIndex)
            {
                GlobalInReference[BoneIndex] = Skeleton.Bones[BoneIndex].GlobalBindPose;
            }

            for (const auto& Pair : BoneNodeToIndex)
            {
                FbxNode*    BoneNode  = Pair.first;
                const int32 BoneIndex = Pair.second;

                if (!BoneNode || BoneIndex < 0 || BoneIndex >= static_cast<int32>(Skeleton.Bones.size()))
                {
                    continue;
                }

                const FMatrix BoneGlobal     = FFbxTransformUtils::ToEngineMatrix(BoneNode->EvaluateGlobalTransform(Time));
                GlobalInReference[BoneIndex] = BoneGlobal * ReferenceMeshBindInverse;
            }

            for (int32 BoneIndex = 0; BoneIndex < static_cast<int32>(Skeleton.Bones.size()); ++BoneIndex)
            {
                if (BoneIndex >= static_cast<int32>(Clip.Tracks.size()))
                {
                    continue;
                }

                FMatrix     LocalMatrix = GlobalInReference[BoneIndex];
                const int32 ParentIndex = Skeleton.Bones[BoneIndex].ParentIndex;

                if (ParentIndex >= 0 && ParentIndex < static_cast<int32>(Skeleton.Bones.size()))
                {
                    LocalMatrix = GlobalInReference[BoneIndex] * GlobalInReference[ParentIndex].GetInverse();
                }

                FBoneTransformKey Key = FFbxTransformUtils::MakeBoneTransformKeyFromEngineMatrix(static_cast<float>(LocalSeconds), LocalMatrix);
                Clip.Tracks[BoneIndex].Keys.push_back(Key);
            }
        };

        for (int32 FrameIndex = 0; FrameIndex <= WholeFrameCount; ++FrameIndex)
        {
            AddAnimationKeysAtTime(static_cast<double>(FrameIndex) / static_cast<double>(SampleRate));
        }

        const double LastWholeFrameSeconds = static_cast<double>(WholeFrameCount) / static_cast<double>(SampleRate);
        if (DurationSeconds - LastWholeFrameSeconds > 1.0e-4)
        {
            AddAnimationKeysAtTime(DurationSeconds);
        }

        ImportBoneRawCurves(AnimStack, BoneNodeToIndex, Skeleton, StartSeconds, EndSeconds, Clip);
        ImportBlendShapeCurves(Scene, AnimStack, StartSeconds, EndSeconds, Clip);
        SortAnimationClipKeys(Clip);

        OutAnimations.push_back(std::move(Clip));
    }
}
