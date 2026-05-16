#include "Animation/AnimDataModel.h"

#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cmath>
#include <utility>

IMPLEMENT_CLASS(UAnimDataModel, UObject)

namespace
{
	float Clamp01(float Value)
	{
		return std::max(0.0f, std::min(1.0f, Value));
	}

	float NormalizeSequenceTime(float Time, float Length)
	{
		if (Length <= 0.0f)
		{
			return 0.0f;
		}

		return std::max(0.0f, std::min(Time, Length));
	}

	FVector SampleVectorKeys(const TArray<FVector>& Keys, float FramePosition, int32 NumberOfKeys, const FVector& DefaultValue)
	{
		if (Keys.empty())
		{
			return DefaultValue;
		}

		if (Keys.size() == 1 || NumberOfKeys <= 1)
		{
			return Keys.front();
		}

		const int32 LastKey = static_cast<int32>(Keys.size()) - 1;
		const float KeyPosition = FramePosition * (static_cast<float>(LastKey) / static_cast<float>(std::max(1, NumberOfKeys - 1)));
		const int32 Key0 = std::max(0, std::min(static_cast<int32>(std::floor(KeyPosition)), LastKey));
		const int32 Key1 = std::max(0, std::min(Key0 + 1, LastKey));
		const float Alpha = Clamp01(KeyPosition - static_cast<float>(Key0));
		return FVector::Lerp(Keys[Key0], Keys[Key1], Alpha);
	}

	FQuat SampleQuatKeys(const TArray<FQuat>& Keys, float FramePosition, int32 NumberOfKeys, const FQuat& DefaultValue)
	{
		if (Keys.empty())
		{
			return DefaultValue;
		}

		if (Keys.size() == 1 || NumberOfKeys <= 1)
		{
			return Keys.front();
		}

		const int32 LastKey = static_cast<int32>(Keys.size()) - 1;
		const float KeyPosition = FramePosition * (static_cast<float>(LastKey) / static_cast<float>(std::max(1, NumberOfKeys - 1)));
		const int32 Key0 = std::max(0, std::min(static_cast<int32>(std::floor(KeyPosition)), LastKey));
		const int32 Key1 = std::max(0, std::min(Key0 + 1, LastKey));
		const float Alpha = Clamp01(KeyPosition - static_cast<float>(Key0));
		return FQuat::Slerp(Keys[Key0], Keys[Key1], Alpha);
	}
}

void UAnimDataModel::Serialize(FArchive& Ar)
{
	Ar << BoneAnimationTracks;
	Ar << PlayLength;
	Ar << FrameRate;
	Ar << NumberOfFrames;
	Ar << NumberOfKeys;
}

const FBoneAnimationTrack* UAnimDataModel::GetBoneTrackByIndex(int32 TrackIndex) const
{
	if (TrackIndex < 0 || TrackIndex >= static_cast<int32>(BoneAnimationTracks.size()))
	{
		return nullptr;
	}

	return &BoneAnimationTracks[TrackIndex];
}

const FBoneAnimationTrack* UAnimDataModel::FindBoneTrackByName(const FName& BoneName) const
{
	for (const FBoneAnimationTrack& Track : BoneAnimationTracks)
	{
		if (Track.Name == BoneName)
		{
			return &Track;
		}
	}

	return nullptr;
}

const FBoneAnimationTrack* UAnimDataModel::FindBoneTrackByIndex(int32 BoneTreeIndex) const
{
	for (const FBoneAnimationTrack& Track : BoneAnimationTracks)
	{
		if (Track.BoneTreeIndex == BoneTreeIndex)
		{
			return &Track;
		}
	}

	return nullptr;
}

bool UAnimDataModel::EvaluateBoneTrackTransform(const FBoneAnimationTrack& Track, float Time, FTransform& OutTransform, const FTransform& DefaultTransform) const
{
	const float EvalTime = NormalizeSequenceTime(Time, PlayLength);
	const int32 KeyCount = std::max(1, NumberOfKeys);
	const float FramePosition = (PlayLength > 0.0f && KeyCount > 1)
		? (EvalTime / PlayLength) * static_cast<float>(KeyCount - 1)
		: 0.0f;

	OutTransform.Location = SampleVectorKeys(Track.InternalTrackData.PosKeys, FramePosition, KeyCount, DefaultTransform.Location);
	OutTransform.Rotation = SampleQuatKeys(Track.InternalTrackData.RotKeys, FramePosition, KeyCount, DefaultTransform.Rotation);
	OutTransform.Scale = SampleVectorKeys(Track.InternalTrackData.ScaleKeys, FramePosition, KeyCount, DefaultTransform.Scale);
	return true;
}
