#include "AnimationStateMachine.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationRuntime.h"
#include "Math/Transform.h"

#include <algorithm>

void UAnimationStateMachine::UpdateAnimationState(float DeltaTime)
{
	// ProcessState() 호출 전에 저장 — EvaluatePose에서 구간 체크에 사용
	PrevStateLocalTime = StateLocalTime;

	if (BlendAlpha < 1.0f)
	{
		BeldingPrevStateTime += (DeltaTime * PlayRate);
		BlendAlpha = (BlendDuration > 0.0f)
			? std::min(BlendAlpha + DeltaTime / BlendDuration, 1.0f)
			: 1.0f;
	}

	ProcessState(DeltaTime);
}

void UAnimationStateMachine::GenerateFinalPose(FPoseContext& OutPose, TArray<FAnimNotifyEvent>& OutNotifies) const
{
	if (!CurrentSequence)
		return;

	TArray<FMatrix> CurrMatrices;
	if (!CurrentSequence->EvaluatePose(StateLocalTime, CurrMatrices))
		return;

	if (BlendAlpha >= 1.0f || !PrevSequence)
	{
		OutPose.BoneLocalTransforms.resize(CurrMatrices.size());
		for (size_t i = 0; i < CurrMatrices.size(); ++i)
			OutPose.BoneLocalTransforms[i] = FTransform(CurrMatrices[i]);
	}
	else
	{
		TArray<FMatrix> PrevMatrices;
		if (!PrevSequence->EvaluatePose(BeldingPrevStateTime, PrevMatrices) || PrevMatrices.size() != CurrMatrices.size())
		{
			OutPose.BoneLocalTransforms.resize(CurrMatrices.size());
			for (size_t i = 0; i < CurrMatrices.size(); ++i)
				OutPose.BoneLocalTransforms[i] = FTransform(CurrMatrices[i]);
		}
		else
		{
			FPoseContext PrevPose, CurrPose;
			PrevPose.BoneLocalTransforms.resize(PrevMatrices.size());
			CurrPose.BoneLocalTransforms.resize(CurrMatrices.size());
			for (size_t i = 0; i < CurrMatrices.size(); ++i)
			{
				PrevPose.BoneLocalTransforms[i] = FTransform(PrevMatrices[i]);
				CurrPose.BoneLocalTransforms[i] = FTransform(CurrMatrices[i]);
			}
			FAnimationRuntime::BlendTwoPoses(PrevPose, CurrPose, BlendAlpha, OutPose);
		}
	}

	if (CurrentSequence)
	{
		CurrentSequence->CollectNotifies(PrevStateLocalTime, StateLocalTime, true, PlayRate < 0.0f, OutNotifies);
	}

	if (BlendAlpha < 1.0f && PrevSequence)
	{
		PrevSequence->CollectNotifies(PrevStateLocalTime, StateLocalTime, true, PlayRate < 0.0f, OutNotifies);
	}
}

UAnimSequence* UAnimationStateMachine::GetCurrentSequence() const
{
	return CurrentSequence;
}

float UAnimationStateMachine::GetCurrentStateTime() const
{
	return StateLocalTime;
}
