#include "Animation/AnimSequence.h"

#include "Mesh/Skeleton.h"
#include "Mesh/SkeletonAsset.h"
#include "Mesh/SkeletonManager.h"
#include "Math/Transform.h"
#include "Object/ObjectFactory.h"

#include <algorithm>
#include <cmath>

IMPLEMENT_CLASS(UAnimSequence, UObject)

namespace
{
	float Clamp01(float Value)
	{
		return std::max(0.0f, std::min(1.0f, Value));
	}

	float NormalizeSequenceTime(float Time, float Length, bool bLoop)
	{
		if (Length <= 0.0f)
		{
			return 0.0f;
		}

		if (bLoop)
		{
			Time = std::fmod(Time, Length);
			if (Time < 0.0f)
			{
				Time += Length;
			}
			return Time;
		}

		return std::max(0.0f, std::min(Time, Length));
	}

	FVector SampleVectorKeys(const TArray<FVector>& Keys, float FramePosition, int32 NumFrames, const FVector& DefaultValue)
	{
		if (Keys.empty())
		{
			return DefaultValue;
		}

		if (Keys.size() == 1 || NumFrames <= 1)
		{
			return Keys.front();
		}

		const int32 LastKey = static_cast<int32>(Keys.size()) - 1;
		const float KeyPosition = FramePosition * (static_cast<float>(LastKey) / static_cast<float>(std::max(1, NumFrames - 1)));
		const int32 Key0 = std::max(0, std::min(static_cast<int32>(std::floor(KeyPosition)), LastKey));
		const int32 Key1 = std::max(0, std::min(Key0 + 1, LastKey));
		const float Alpha = Clamp01(KeyPosition - static_cast<float>(Key0));
		return FVector::Lerp(Keys[Key0], Keys[Key1], Alpha);
	}

	FQuat SampleQuatKeys(const TArray<FQuat>& Keys, float FramePosition, int32 NumFrames, const FQuat& DefaultValue)
	{
		if (Keys.empty())
		{
			return DefaultValue;
		}

		if (Keys.size() == 1 || NumFrames <= 1)
		{
			return Keys.front();
		}

		const int32 LastKey = static_cast<int32>(Keys.size()) - 1;
		const float KeyPosition = FramePosition * (static_cast<float>(LastKey) / static_cast<float>(std::max(1, NumFrames - 1)));
		const int32 Key0 = std::max(0, std::min(static_cast<int32>(std::floor(KeyPosition)), LastKey));
		const int32 Key1 = std::max(0, std::min(Key0 + 1, LastKey));
		const float Alpha = Clamp01(KeyPosition - static_cast<float>(Key0));
		return FQuat::Slerp(Keys[Key0], Keys[Key1], Alpha);
	}
}

void UAnimSequence::Serialize(FArchive& Ar)
{
	if (Ar.IsLoading() && !AnimSequenceAsset)
	{
		AnimSequenceAsset = new FAnimSequenceAsset();
	}

	AnimSequenceAsset->Serialize(Ar);

	if (Ar.IsLoading())
	{
		ResolveSkeleton();
	}
}

void UAnimSequence::SetAnimSequenceAsset(FAnimSequenceAsset* InAsset)
{
	AnimSequenceAsset = InAsset;
	ResolveSkeleton();
}

FAnimSequenceAsset* UAnimSequence::GetAnimSequenceAsset() const
{
	return AnimSequenceAsset;
}

void UAnimSequence::SetSkeleton(USkeleton* InSkeleton)
{
	Skeleton = InSkeleton;
	if (AnimSequenceAsset && Skeleton)
	{
		AnimSequenceAsset->SkeletonPath = Skeleton->GetAssetPathFileName();
	}
}

USkeleton* UAnimSequence::GetSkeleton() const
{
	return Skeleton;
}

FSkeletonAsset* UAnimSequence::GetSkeletonAsset() const
{
	return Skeleton ? Skeleton->GetSkeletonAsset() : nullptr;
}

bool UAnimSequence::ResolveSkeleton()
{
	if (!AnimSequenceAsset)
	{
		Skeleton = nullptr;
		return false;
	}

	if (Skeleton && Skeleton->GetAssetPathFileName() == AnimSequenceAsset->SkeletonPath)
	{
		return true;
	}

	Skeleton = nullptr;
	if (AnimSequenceAsset->SkeletonPath.empty())
	{
		return false;
	}

	Skeleton = FSkeletonManager::Get().Load(AnimSequenceAsset->SkeletonPath);
	return Skeleton != nullptr;
}

bool UAnimSequence::EvaluatePose(float Time, TArray<FMatrix>& OutLocalMatrices, bool bLoop) const
{
	OutLocalMatrices.clear();

	const FSkeletonAsset* SkeletonAsset = GetSkeletonAsset();
	if (!AnimSequenceAsset || !SkeletonAsset)
	{
		return false;
	}

	const int32 BoneCount = static_cast<int32>(SkeletonAsset->Bones.size());
	OutLocalMatrices.resize(BoneCount);
	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		OutLocalMatrices[BoneIndex] = SkeletonAsset->Bones[BoneIndex].LocalMatrix;
	}

	if (BoneCount == 0 || AnimSequenceAsset->Tracks.empty())
	{
		return true;
	}

	const float SequenceLength = AnimSequenceAsset->SequenceLength;
	const int32 NumFrames = std::max(1, AnimSequenceAsset->NumFrames);
	const float EvalTime = NormalizeSequenceTime(Time, SequenceLength, bLoop);
	const float FramePosition = (SequenceLength > 0.0f && NumFrames > 1)
		? (EvalTime / SequenceLength) * static_cast<float>(NumFrames - 1)
		: 0.0f;

	for (const FBoneAnimationTrack& Track : AnimSequenceAsset->Tracks)
	{
		if (Track.BoneIndex < 0 || Track.BoneIndex >= BoneCount)
		{
			continue;
		}

		const FTransform RefTransform(SkeletonAsset->Bones[Track.BoneIndex].LocalMatrix);
		FTransform SampledTransform;
		SampledTransform.Location = SampleVectorKeys(Track.InternalTrack.PosKeys, FramePosition, NumFrames, RefTransform.Location);
		SampledTransform.Rotation = SampleQuatKeys(Track.InternalTrack.RotKeys, FramePosition, NumFrames, RefTransform.Rotation);
		SampledTransform.Scale = SampleVectorKeys(Track.InternalTrack.ScaleKeys, FramePosition, NumFrames, RefTransform.Scale);
		OutLocalMatrices[Track.BoneIndex] = SampledTransform.ToMatrix();
	}

	return true;
}
