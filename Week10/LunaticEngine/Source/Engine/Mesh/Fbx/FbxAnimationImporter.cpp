#include "Mesh/Fbx/FbxAnimationImporter.h"

#include "Mesh/Fbx/FbxTransformUtils.h"

#include <fbxsdk.h>

#include <cmath>
#include <utility>

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

    const float SampleRate = 30.0f;

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

        // 지정 시간의 bone global transform을 reference space local key로 샘플링한다.
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

        OutAnimations.push_back(std::move(Clip));
    }
}
