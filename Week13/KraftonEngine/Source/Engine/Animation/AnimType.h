#pragma once
#include "Math/Transform.h"
#include "Object/FName.h"

struct FPoseContext
{
	// 부모 bone 기준의 Local Transform
	TArray<FTransform> BoneLocalTransforms;
};

struct FAnimNotifyEvent
{
	float TriggerTime; // 언제 실행될지 (0~1사이)
	float Duration;    // 지속시간
	FName NotifyName;  // 이벤트 이름
};