#include "CoreTypes.h"
#include "Object.h"
#include "EngineStatics.h"

uint32 UEngineStatics::NextUUID = 0;
TArray<UObject *> GUObjectArray;

UObject::UObject()
{
    UUID = UEngineStatics::GenUUID();

    GUObjectArray.push_back(this);
    InternalIndex = static_cast<uint32>(GUObjectArray.size()) - 1;
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

UWorld *UObject::GetWorld() const
{
    if (Outer != nullptr)
    {
        return Outer->GetWorld();
    }

    return nullptr;
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

