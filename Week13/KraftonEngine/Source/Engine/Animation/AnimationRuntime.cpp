#include "AnimationRuntime.h"

FTransform FAnimationRuntime::GetTrackTransformAtTime(const FBoneAnimationTrack& Track, float Time, const FFrameRate& FrameRate, uint32 NumberOfKeys)
{
	const FRawAnimSequenceTrack& Raw = Track.InternalTrack;

	if (NumberOfKeys == 0)
		return FTransform();

	// Frame 기반 인덱스 계산
	float FrameTime = std::max(0.0f, Time * FrameRate.AsDecimal());
	uint32 PrevIndex = FMath::Clamp((uint32)FrameTime, 0u, NumberOfKeys - 1);
	uint32 NextIndex = std::min(PrevIndex + 1, NumberOfKeys - 1);

	float Alpha = FrameTime - PrevIndex;

	// 위치 보간
	FVector Pos = Raw.PosKeys[PrevIndex];
	if (PrevIndex != NextIndex)
		Pos = FVector::Lerp(Raw.PosKeys[PrevIndex], Raw.PosKeys[NextIndex], Alpha);

	// 회전 보간
	FQuat Rot = Raw.RotKeys[PrevIndex];
	if (PrevIndex != NextIndex)
		Rot = FQuat::Slerp(Raw.RotKeys[PrevIndex], Raw.RotKeys[NextIndex], Alpha);

	// 크기 보간
	FVector Scale = Raw.ScaleKeys[PrevIndex];
	if (PrevIndex != NextIndex)
		Scale = FVector::Lerp(Raw.ScaleKeys[PrevIndex], Raw.ScaleKeys[NextIndex], Alpha);

	return FTransform(Pos, Rot, Scale);
}

void FAnimationRuntime::GetPoseAtTime(const UAnimDataModel* DataModel, float Time, FPoseContext& OutPose)
{
	if (!DataModel)
		return;

	const auto& Tracks = DataModel->GetBoneAnimationTracks();

	OutPose.BoneLocalTransforms.resize(Tracks.size());

	for (uint32 Idx = 0; Idx < static_cast<uint32>(Tracks.size()); ++Idx)
	{
		const FBoneAnimationTrack& Track = Tracks[Idx];
		const uint32 NumKeys = static_cast<uint32>(Track.InternalTrack.PosKeys.size());
		OutPose.BoneLocalTransforms[Idx] = GetTrackTransformAtTime(Track, Time, DataModel->GetFrameRate(), NumKeys);
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
