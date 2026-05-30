#pragma once
#include "Core/ClassTypes.h"

class FArchive;
class UClass;

struct FSoftObjectPath
{
private:
	FString AssetPathName;

public:
	FSoftObjectPath() = default;
	FSoftObjectPath(const FString& InPath)
		: AssetPathName(InPath)
	{
	}

	bool IsNull() const
	{
		return AssetPathName.empty() || AssetPathName == "None";
	}

	void Reset()
	{
		AssetPathName = "None";
	}

	const FString& ToString() const
	{
		return AssetPathName;
	}

	void SetPath(const FString& InPath)
	{
		AssetPathName = InPath.empty() ? "None" : InPath;
	}

	UObject* ResolveObject() const;
	UObject* ResolveObject(const UClass* ExpectedClass) const;
};

FArchive& operator<<(FArchive& Ar, FSoftObjectPath& Path);
