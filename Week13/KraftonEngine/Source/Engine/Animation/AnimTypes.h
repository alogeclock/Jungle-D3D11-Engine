#pragma once

#include "Core/CoreTypes.h"
#include "Math/Quat.h"
#include "Math/Vector.h"
#include "Serialization/Archive.h"

/** 한 Bone의 transform key 데이터만 들고 있는 순수 데이터 구조체 입니다. */
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

/** FRawAnimSequenceTrack이 어떤 Bone에 속하는지 연결합니다. */
struct FBoneAnimationTrack
{
	FRawAnimSequenceTrack InternalTrackData;
	int32 BoneTreeIndex = -1;
	FName Name;

	friend FArchive& operator<<(FArchive& Ar, FBoneAnimationTrack& Track)
	{
		Ar << Track.InternalTrackData;
		Ar << Track.BoneTreeIndex;
		Ar << Track.Name;
		return Ar;
	}
};
