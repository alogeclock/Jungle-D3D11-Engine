#include "Source/Core/Public/FName.h"

FName FNamePool::AddName(const FString& InString)
{
	if (!NameToUniqueIndex.contains(InString))
	{
		uint32 uniqueNameKey = NameToUniqueIndex.size();
		NameToUniqueIndex[InString] = uniqueNameKey;
		UniqueNameArray.push_back(InString);
	}

	size_t underscorePos = InString.find_last_of('_');
	FString Name = InString;

	for (int i = 0; i < Name.size(); i++)
	{
		if (Name[i] < 'a' || Name[i] > 'z')
			continue;
		Name[i] = std::tolower(Name[i]);
	}

	if (!ComparsionPool.contains(Name))
	{
		uint32 key = ComparsionPool.size();
		ComparsionPool[Name] = key;
	}

	return FName(ComparsionPool[Name], NameToUniqueIndex[InString]);
}

FString FNamePool::GetName(uint32 InNameIndex)
{
	return UniqueNameArray[InNameIndex];
}
