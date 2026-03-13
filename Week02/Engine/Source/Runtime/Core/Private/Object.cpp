#include "Engine\Source\Runtime\Core\Public\Object.h"

UObject::UObject()
{
}

UObject::UObject(const FString& InString): Name(InString)
{
}

const FString& UObject::GetName() const 
{ 
	return Name; 
}

void UObject::SetName(const FString& InName) 
{ 
	Name = InName; 
}

uint64 UObject::GetAllocatedBytes() const 
{ 
	return AllocatedBytes; 
}

uint32 UObject::GetAllocatedCount() const 
{ 
	return AllocatedCounts; 
}

void UObject::AddMemoryUsage(uint64 InBytes, uint32 InCount)
{
	AllocatedBytes += InBytes;
	AllocatedCounts += InCount;
}

void UObject::RemoveMemoryUsage(uint64 InBytes, uint32 InCount)
{
	if (AllocatedBytes >= InBytes)
	{
		AllocatedBytes -= InBytes;
	}
	if (AllocatedCounts >= InCount)
	{
		AllocatedCounts -= InCount;
	}
}

