#pragma once
#include "Math/Transform.h"
#include "Object/FName.h"

struct FPoseContext
{
	// 부모 bone 기준의 Local Transform
	TArray<FTransform> BoneLocalTransforms;
	// 본 이름을 기준으로 BoneLocalTransforms의 인덱스 저장
	TMap<FName, uint32> BoneNameToIndex;

	void SetBoneTransform(const FName& Name, const FTransform& T)
	{
		auto It = BoneNameToIndex.find(Name);
		if (It != BoneNameToIndex.end())
			BoneLocalTransforms[It->second] = T;
	}

	void Reset()
	{
		BoneLocalTransforms.clear();
	}
};

struct FAnimNotifyEvent
{
	float TriggerTime; // 언제 실행될지 (0~1사이)
	float Duration;    // 지속시간
	FName NotifyName;  // 이벤트 이름
};