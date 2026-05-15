#pragma once
#include "Math/Transform.h"
#include "Object/FName.h"

struct FPoseContext
{
	// 부모 bone 기준의 Local Transform (Skeleton bone 순서와 동일한 index로 접근)
	TArray<FTransform> BoneLocalTransforms;

	void Reset()
	{
		BoneLocalTransforms.clear();
	}
};

struct FAnimNotifyEvent
{
	float TriggerTime = 0.0f; // 언제 실행될지 초단위
	float Duration = 0.0f;    // 지속시간
	FName NotifyName;  // 이벤트 이름
};