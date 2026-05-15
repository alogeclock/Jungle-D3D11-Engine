#include "AnimationRuntime.h"

FTransform FAnimationRuntime::GetTrackTransformAtTime(const FBoneAnimationTrack& Track, float Time, const FFrameRate& FrameRate, uint32 NumberOfKeys)
{
    return FTransform();
}

void FAnimationRuntime::GetPoseAtTime(const UAnimDataModel* DataModel, float Time, FPoseContext& OutPose)
{

}

void FAnimationRuntime::BlendTwoPoses(const FPoseContext& A, const FPoseContext& B, float Alpha, FPoseContext& OutPose)
{
}
