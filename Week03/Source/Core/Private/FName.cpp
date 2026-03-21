#include "Source/Core/Public/FName.h"

FName::FName() : ComparisionIndex(0), Number(0), DisplayIndex(0)
{
}

FName::FName(char* pStr) : FName(FString(pStr))
{	
}

FName::FName(const FString& Name)
{
	FName name = GNamePool.AddName(Name);
	ComparisionIndex = name.ComparisionIndex;
	DisplayIndex = name.DisplayIndex;
}

FName::FName(uint32 InComparisionIndex, uint32 InDisplayIndex)
{
	ComparisionIndex = InComparisionIndex;
	DisplayIndex = InDisplayIndex;
}

bool FName::operator==(const FName& Other) const
{
	return ComparisionIndex == Other.ComparisionIndex;
}

FString FName::operator+(const FString& Other)
{
	return ToString() + Other;
}

int32 FName::Compare(const FName& Other) const
{
	FString ThisString = GNamePool.GetName(DisplayIndex);
	FString OtherString = GNamePool.GetName(Other.DisplayIndex);
	return ThisString.compare(OtherString);
}

FString FName::ToString() const
{
	FString StringName = GNamePool.GetName(DisplayIndex);
	if (Number > 0)
		StringName += "_" + std::to_string(Number);

	return StringName;
}

void FName::SetNumber(uint32 InNumber)
{
	Number = InNumber;
}

uint32 FName::GetDisplayIndex()
{
	return DisplayIndex;
}
