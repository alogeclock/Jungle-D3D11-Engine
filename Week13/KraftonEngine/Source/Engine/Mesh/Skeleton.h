#pragma once

#include "Object/Object.h"
#include "Mesh/SkeletonAsset.h"

class USkeleton : public UObject
{
public:
	DECLARE_CLASS(USkeleton, UObject)

	USkeleton() = default;
	~USkeleton() override = default;

	void Serialize(FArchive& Ar);

	const FString& GetAssetPathFileName() const { return AssetPathFileName; }
	void SetAssetPathFileName(const FString& InPathFileName) { AssetPathFileName = InPathFileName; }

	void SetSkeletonAsset(FSkeletonAsset* InAsset);
	FSkeletonAsset* GetSkeletonAsset() const;

private:
	FString AssetPathFileName = "None";
	FSkeletonAsset* SkeletonAsset = nullptr;
};
