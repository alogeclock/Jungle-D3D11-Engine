#pragma once
#include "Animation/AnimType.h"

struct FBoneAnimationTrack; // 특정 Bone 1개에 대한 애니메이션 키프레임 데이터 집합
struct FFrameRate;
class UAnimDataModel;

namespace FAnimationRuntime
{
	// FBoneAnimationTrack 하나에서 time 기준 보간값 추출
	FTransform GetTrackTransformAtTime(
		const FBoneAnimationTrack& Track,
		float Time,
		const FFrameRate& FrameRate,
		uint32 NumberOfKeys);

	// 위 함수를 전체 트랙에 적용 -> FPoseContext 완성
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
