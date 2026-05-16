#include "AnimationRuntime.h"
#include "Animation/AnimDataModel.h"

void FAnimationRuntime::GetPoseAtTime(const UAnimDataModel* DataModel, float Time, FPoseContext& OutPose)
{
	if (!DataModel)
		return;

	const TArray<FBoneAnimationTrack>& Tracks = DataModel->GetBoneAnimationTracks();
	OutPose.BoneLocalTransforms.resize(Tracks.size());

	for (uint32 Idx = 0; Idx < static_cast<uint32>(Tracks.size()); ++Idx)
	{
		FTransform Result;
		DataModel->EvaluateBoneTrackTransform(Tracks[Idx], Time, Result, FTransform());
		OutPose.BoneLocalTransforms[Idx] = Result;
	}
}

void FAnimationRuntime::BlendTwoPoses(const FPoseContext& A, const FPoseContext& B, float Alpha, FPoseContext& OutPose)
{
	const uint32 NumBones = static_cast<uint32>(A.BoneLocalTransforms.size());
	if (NumBones != static_cast<uint32>(B.BoneLocalTransforms.size()))
		return;

	OutPose.BoneLocalTransforms.resize(NumBones);

	for (uint32 i = 0; i < NumBones; ++i)
	{
		const FTransform& TA = A.BoneLocalTransforms[i];
		const FTransform& TB = B.BoneLocalTransforms[i];

		OutPose.BoneLocalTransforms[i] = FTransform(
			FVector::Lerp(TA.Location, TB.Location, Alpha),
			FQuat::Slerp(TA.Rotation, TB.Rotation, Alpha),
			FVector::Lerp(TA.Scale, TB.Scale, Alpha)
		);
	}
}
