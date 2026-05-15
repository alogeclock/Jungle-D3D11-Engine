#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"
#include "Math/Quat.h"
#include "Math/Vector.h"
#include "Object/FName.h"
#include "Serialization/Archive.h"

/**
 * 하나의 Bone의 raw transform key 데이터입니다.
 * FBX에서 샘플링한 local transform이 여기에 bake 됩니다.
 * 모든 키가 같으면 1개 키로 줄어듭니다.
 */
struct FRawAnimSequenceTrack
{
	TArray<FVector> PosKeys;
	TArray<FQuat> RotKeys;
	TArray<FVector> ScaleKeys;

	friend FArchive& operator<<(FArchive& Ar, FRawAnimSequenceTrack& Track)
	{
		Ar << Track.PosKeys;
		Ar << Track.RotKeys;
		Ar << Track.ScaleKeys;
		return Ar;
	}
};

/**
 * FRawAnimSequenceTrack이 어떤 bone에 해당하는지 묶어주는 단위입니다.
 * Name은 Bone이름, BoneIndex는 Skeleton의 bone index입니다.
 * 순수 데이터 컨테이너입니다.
 */
struct FBoneAnimationTrack
{
	FName Name;
	int32 BoneIndex = -1;
	FRawAnimSequenceTrack InternalTrack;

	friend FArchive& operator<<(FArchive& Ar, FBoneAnimationTrack& Track)
	{
		Ar << Track.Name;
		Ar << Track.BoneIndex;
		Ar << Track.InternalTrack;
		return Ar;
	}
};

/** .uasset에 실제로 serialize되는 AnimSequence 데이터입니다. */
struct FAnimSequenceAsset
{
	FString PathFileName;
	FString SkeletonPath;
	float SequenceLength = 0.0f;
	float SampleRate = 30.0f;
	int32 NumFrames = 0;
	TArray<FBoneAnimationTrack> Tracks;

	void Serialize(FArchive& Ar)
	{
		Ar << PathFileName;
		Ar << SkeletonPath;
		Ar << SequenceLength;
		Ar << SampleRate;
		Ar << NumFrames;
		Ar << Tracks;
	}
};
