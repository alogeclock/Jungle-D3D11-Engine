#pragma once

#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimTypes.h"

class UAnimDataModel;

/** 실제 단일 Animation Clip입니다. */
class UAnimSequence : public UAnimSequenceBase
{
public:
	DECLARE_CLASS(UAnimSequence, UAnimSequenceBase)

	UAnimSequence() = default;
	~UAnimSequence() override = default;

	void Serialize(FArchive& Ar) override;

	void SetDataModel(UAnimDataModel* InDataModel);
	UAnimDataModel* GetDataModel() const { return DataModel; }

	int32 GetNumberOfSampledKeys() const override;
	float GetSamplingFrameRate() const override;
	bool EvaluatePose(float Time, TArray<FMatrix>& OutLocalMatrices, bool bLoopOverride = true) const override;

	const TArray<FAnimNotifyEvent>& GetNotifyEvents() const;

private:
	UAnimDataModel* DataModel = nullptr;
};
