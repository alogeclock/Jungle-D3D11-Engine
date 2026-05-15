#pragma once

#include "Animation/AnimSequenceAsset.h"
#include "Object/Object.h"

class USkeleton;
struct FSkeletonAsset;

/** 런타임에서 쓰는 UObject wrapper입니다. */
class UAnimSequence : public UObject
{
public:
	DECLARE_CLASS(UAnimSequence, UObject)

	UAnimSequence() = default;
	~UAnimSequence() override = default;

	void Serialize(FArchive& Ar) override;

	const FString& GetAssetPathFileName() const { return AssetPathFileName; }
	void SetAssetPathFileName(const FString& InPathFileName) { AssetPathFileName = InPathFileName; }

	void SetAnimSequenceAsset(FAnimSequenceAsset* InAsset);
	FAnimSequenceAsset* GetAnimSequenceAsset() const;

	void SetSkeleton(USkeleton* InSkeleton);
	USkeleton* GetSkeleton() const;
	FSkeletonAsset* GetSkeletonAsset() const;
	bool ResolveSkeleton();

	bool EvaluatePose(float Time, TArray<FMatrix>& OutLocalMatrices, bool bLoop = true) const;

private:
	FAnimSequenceAsset* AnimSequenceAsset = nullptr;
	USkeleton* Skeleton = nullptr;
	FString AssetPathFileName;
};
