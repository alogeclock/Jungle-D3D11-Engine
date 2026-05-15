#pragma once
#include "Animation/AnimType.h"

struct FBoneAnimationTrack;
struct FFrameRate;
class UAnimDataModel;

namespace FAnimationRuntime
{
	FTransform GetTrackTransformAtTime(
		const FBoneAnimationTrack& Track,
		float Time,
		const FFrameRate& FrameRate,
		uint32 NumberOfKeys);

	void GetPoseAtTime(
		const UAnimDataModel* DataModel,
		float Time,
		FPoseContext& OutPose);

	void BlendTwoPoses(
		const FPoseContext& A,
		const FPoseContext& B,
		float Alpha,
		FPoseContext& OutPose);
};
