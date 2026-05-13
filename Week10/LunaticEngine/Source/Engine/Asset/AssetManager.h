#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"

class USkeletalMesh;

struct FAssetReference
{
	FString Path = "None";

	bool IsNull() const
	{
		return Path.empty() || Path == "None";
	}
};

class FAssetManager : public TSingleton<FAssetManager>
{
	friend class TSingleton<FAssetManager>;

public:
	USkeletalMesh* LoadSkeletalMesh(const FAssetReference& Reference);

private:
	FAssetManager() = default;
};
