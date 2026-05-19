#pragma once

#include "Core/CoreTypes.h"
#include "Core/PropertyTypes.h"
#include "Math/Quat.h"
#include "Math/Transform.h"
#include "Math/Vector.h"
#include "Object/FName.h"
#include "Serialization/Archive.h"
#include "Notify.h"

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

struct FPoseContext
{
	// 부모 bone 기준의 Local Transform (Skeleton bone 순서와 동일한 index로 접근)
	TArray<FTransform> BoneLocalTransforms;

	// TODO: IK를 위한 TArray<FMatrix> GlobalTransforms 추가 해야함.

	void Reset()
	{
		BoneLocalTransforms.clear();
	}
};

struct FAnimNotifyEvent
{
	float TriggerTime = 0.0f;
	float Duration    = 0.0f;
	FName NotifyName;

	UNotify* NotifyTrigger = nullptr;

	bool IsStateNotify() const { return Duration > 0.0f; }

	friend FArchive& operator<<(FArchive& Ar, FAnimNotifyEvent& Notify)
	{
		Ar << Notify.TriggerTime;
		Ar << Notify.Duration;
		Ar << Notify.NotifyName;
		return Ar;
	}
};

struct FTwoBoneIKChain
{
	bool bEnabled = true;

	/** UpperArm or Thigh */
	int RootBoneIndex = -1;

	/** LowerArm or Calf */
	int MidBoneIndex = -1;

	/** Hand or Foot */
	int EndBoneIndex = -1;

	FVector TargetPosition = FVector::ZeroVector;
	FVector PolePosition = FVector::ForwardVector;

	static void DescribeProperties(void* Ptr, std::vector<FProperty>& OutProps)
	{
		auto* Chain = static_cast<FTwoBoneIKChain*>(Ptr);

		FProperty Enabled;
		Enabled.Name = "Enabled";
		Enabled.Type = EPropertyType::Bool;
		Enabled.ValuePtr = &Chain->bEnabled;
		OutProps.push_back(Enabled);

		FProperty RootBone;
		RootBone.Name = "Root Bone Index";
		RootBone.Type = EPropertyType::Int;
		RootBone.Speed = 1.0f;
		RootBone.ValuePtr = &Chain->RootBoneIndex;
		OutProps.push_back(RootBone);

		FProperty MidBone;
		MidBone.Name = "Mid Bone Index";
		MidBone.Type = EPropertyType::Int;
		MidBone.Speed = 1.0f;
		MidBone.ValuePtr = &Chain->MidBoneIndex;
		OutProps.push_back(MidBone);

		FProperty EndBone;
		EndBone.Name = "End Bone Index";
		EndBone.Type = EPropertyType::Int;
		EndBone.Speed = 1.0f;
		EndBone.ValuePtr = &Chain->EndBoneIndex;
		OutProps.push_back(EndBone);

		FProperty Target;
		Target.Name = "Target Position";
		Target.Type = EPropertyType::Vec3;
		Target.Speed = 1.0f;
		Target.ValuePtr = &Chain->TargetPosition;
		OutProps.push_back(Target);

		FProperty Pole;
		Pole.Name = "Pole Position";
		Pole.Type = EPropertyType::Vec3;
		Pole.Speed = 1.0f;
		Pole.ValuePtr = &Chain->PolePosition;
		OutProps.push_back(Pole);
	}

	friend FArchive& operator<<(FArchive& Ar, FTwoBoneIKChain& Chain)
	{
		Ar << Chain.bEnabled;
		Ar << Chain.RootBoneIndex;
		Ar << Chain.MidBoneIndex;
		Ar << Chain.EndBoneIndex;
		Ar << Chain.TargetPosition;
		Ar << Chain.PolePosition;
		return Ar;
	}
};
