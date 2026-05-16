#pragma once
#include "Animation/AnimTypes.h"

class UAnimDataModel;

namespace FAnimationRuntime
{
	// FPoseContext 완성
	void GetPoseAtTime(
		const UAnimDataModel* DataModel,
		float Time,
		FPoseContext& OutPose);

	// 두 포즈 블렌딩
	void BlendTwoPoses(
		const FPoseContext& A,
		const FPoseContext& B,
		float Alpha,
		FPoseContext& OutPose);
};
