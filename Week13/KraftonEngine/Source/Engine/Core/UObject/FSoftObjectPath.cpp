#include "Core/UObject/FSoftObjectPath.h"

#include "Object/Object.h"
#include "Serialization/Archive.h"

FArchive& operator<<(FArchive& Ar, FSoftObjectPath& Path)
{
	FString SerializedPath = Ar.IsSaving() ? Path.ToString() : FString();
	Ar << SerializedPath;

	if (Ar.IsLoading())
	{
		Path.SetPath(SerializedPath);
	}

	return Ar;
}

UObject* FSoftObjectPath::ResolveObject() const
{
	return ResolveObject(nullptr);
}

UObject* FSoftObjectPath::ResolveObject(const UClass* ExpectedClass) const
{
	if (IsNull()) return nullptr;

	for (const FUObjectItem& Item : GUObjectArray.GetItems())
	{
		UObject* Object = Item.Object;
		if (!Object) continue;
		if (ExpectedClass && !Object->IsA(ExpectedClass)) continue;
		if (Object->GetAssetPathFileName() == AssetPathName) return Object;
	}

	for (const FUObjectItem& Item : GUObjectArray.GetItems())
	{
		UObject* Object = Item.Object;
		if (!Object) continue;
		if (ExpectedClass && !Object->IsA(ExpectedClass)) continue;
		if (Object->GetFName().ToString() == AssetPathName) return Object;
	}

	return nullptr;
}
