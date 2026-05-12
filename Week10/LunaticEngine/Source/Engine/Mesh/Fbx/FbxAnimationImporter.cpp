#include "Mesh/Fbx/FbxAnimationImporter.h"

#include "Mesh/Fbx/FbxSceneQuery.h"
#include "Mesh/Fbx/FbxTransformUtils.h"

#include <fbxsdk.h>

#include <cmath>
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

    static int32 FindOrAddFloatCurve(FSkeletalAnimationClip& Clip, const FString& CurveName)
    {
        for (int32 CurveIndex = 0; CurveIndex < static_cast<int32>(Clip.FloatCurves.size()); ++CurveIndex)
        {
            if (Clip.FloatCurves[CurveIndex].Name == CurveName)
            {
                return CurveIndex;
            }
        }

        FAnimationFloatCurve NewCurve;
        NewCurve.Name = CurveName;
        Clip.FloatCurves.push_back(std::move(NewCurve));
        return static_cast<int32>(Clip.FloatCurves.size()) - 1;
    }

    static void ImportBlendShapeCurvesForLayer(FbxScene* Scene, FbxAnimLayer* AnimLayer, double StartSeconds, double EndSeconds, FSkeletalAnimationClip& Clip)
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

                    FString CurveName = Channel->GetName();
                    if (CurveName.empty())
                    {
                        CurveName = MeshNode->GetName();
                    }

                    const int32           CurveIndex = FindOrAddFloatCurve(Clip, CurveName);
                    FAnimationFloatCurve& OutCurve   = Clip.FloatCurves[CurveIndex];

                    const int32 KeyCount = Curve->KeyGetCount();
                    for (int32 KeyIndex = 0; KeyIndex < KeyCount; ++KeyIndex)
                    {
                        const double AbsoluteSeconds = Curve->KeyGetTime(KeyIndex).GetSecondDouble();

                        if (AbsoluteSeconds < StartSeconds - 1.0e-6 || AbsoluteSeconds > EndSeconds + 1.0e-6)
                        {
                            continue;
                        }

                        FFloatCurveKey Key;
                        Key.TimeSeconds = static_cast<float>(AbsoluteSeconds - StartSeconds);
                        Key.Value       = static_cast<float>(Curve->KeyGetValue(KeyIndex));

                        OutCurve.Keys.push_back(Key);
                    }
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
            ImportBlendShapeCurvesForLayer(Scene, AnimLayer, StartSeconds, EndSeconds, Clip);
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

        FbxTimeSpan TimeSpan = AnimStack->GetLocalTimeSpan();

        const double StartSeconds = TimeSpan.GetStart().GetSecondDouble();
        const double EndSeconds   = TimeSpan.GetStop().GetSecondDouble();

        if (EndSeconds <= StartSeconds)
        {
            continue;
        }

        FSkeletalAnimationClip Clip;
        Clip.Name            = AnimStack->GetName();
        Clip.DurationSeconds = static_cast<float>(EndSeconds - StartSeconds);
        Clip.SampleRate      = SampleRate;

        Clip.Tracks.resize(Skeleton.Bones.size());

        for (const auto& Pair : BoneNodeToIndex)
        {
            const int32 BoneIndex = Pair.second;

            if (BoneIndex < 0 || BoneIndex >= static_cast<int32>(Skeleton.Bones.size()))
            {
                continue;
            }

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

        ImportBlendShapeCurves(Scene, AnimStack, StartSeconds, EndSeconds, Clip);

        OutAnimations.push_back(std::move(Clip));
    }
}
