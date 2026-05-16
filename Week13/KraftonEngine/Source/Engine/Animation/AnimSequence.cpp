#include "Animation/AnimSequence.h"

#include "Animation/AnimDataModel.h"
#include "Mesh/SkeletonAsset.h"
#include "Math/Transform.h"
#include "Object/ObjectFactory.h"

#include <algorithm>
#include <cmath>

IMPLEMENT_CLASS(UAnimSequence, UAnimSequenceBase)

namespace
{
	float NormalizeSequenceTime(float Time, float Length, bool bLoop)
	{
		if (Length <= 0.0f)
		{
			return 0.0f;
		}

		if (bLoop)
		{
			Time = std::fmod(Time, Length);
			if (Time < 0.0f)
			{
				Time += Length;
			}
			return Time;
		}

		return std::max(0.0f, std::min(Time, Length));
	}
}

void UAnimSequence::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (!DataModel)
	{
		DataModel = UObjectManager::Get().CreateObject<UAnimDataModel>(this);
	}

	DataModel->Serialize(Ar);

	if (Ar.IsLoading())
	{
		SetSequenceLength(DataModel->GetPlayLength());
		ResolveSkeleton();
	}
}

void UAnimSequence::SetDataModel(UAnimDataModel* InDataModel)
{
	DataModel = InDataModel;
	if (DataModel)
	{
		SetSequenceLength(DataModel->GetPlayLength());
	}
}

const TArray<FAnimNotifyEvent>& UAnimSequence::GetNotifyEvents() const
{
	static const TArray<FAnimNotifyEvent> Empty;
	return DataModel ? DataModel->GetNotifies() : Empty;
}

int32 UAnimSequence::GetNumberOfSampledKeys() const
{
	return DataModel ? DataModel->GetNumberOfKeys() : 0;
}

float UAnimSequence::GetSamplingFrameRate() const
{
	return DataModel ? DataModel->GetFrameRate() : 0.0f;
}

bool UAnimSequence::EvaluatePose(float Time, TArray<FMatrix>& OutLocalMatrices, bool bLoopOverride) const
{
	OutLocalMatrices.clear();

	const FSkeletonAsset* SkeletonAsset = GetSkeletonAsset();
	if (!DataModel || !SkeletonAsset)
	{
		return false;
	}

	const int32 BoneCount = static_cast<int32>(SkeletonAsset->Bones.size());
	OutLocalMatrices.resize(BoneCount);
	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		OutLocalMatrices[BoneIndex] = SkeletonAsset->Bones[BoneIndex].LocalMatrix;
	}

	if (BoneCount == 0 || DataModel->GetBoneAnimationTracks().empty())
	{
		return true;
	}

	const float EvalTime = NormalizeSequenceTime(Time, GetPlayLength(), bLoopOverride && IsLooping());
	for (const FBoneAnimationTrack& Track : DataModel->GetBoneAnimationTracks())
	{
		if (Track.BoneTreeIndex < 0 || Track.BoneTreeIndex >= BoneCount)
		{
			continue;
		}

		const FTransform RefTransform(SkeletonAsset->Bones[Track.BoneTreeIndex].LocalMatrix);
		FTransform SampledTransform;
		DataModel->EvaluateBoneTrackTransform(Track, EvalTime, SampledTransform, RefTransform);
		OutLocalMatrices[Track.BoneTreeIndex] = SampledTransform.ToMatrix();
	}

	return true;
}
