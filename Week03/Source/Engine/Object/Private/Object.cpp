#include "CoreTypes.h"
#include "../Public/Object.h"
#include "EngineStatics.h"
#include <Windows.h>


uint32 UEngineStatics::NextUUID = 0;
TArray<UObject *> GUObjectArray;

UObject::UObject(const FString &InString) : Name(InString), Outer(nullptr)
{
    UUID = UEngineStatics::GenUUID();

    GUObjectArray.push_back(this);
    InternalIndex = static_cast<uint32>(GUObjectArray.size()) - 1;

    //SetName(InString);
}

UObject::~UObject()
{
    auto it = std::find(GUObjectArray.begin(), GUObjectArray.end(), this);
    if (it != GUObjectArray.end())
    {
        *it = GUObjectArray.back();
        GUObjectArray.pop_back();
    }
}

const FName& UObject::GetName() const
{ 
	return Name; 
}

void UObject::SetName(const FString& InName) 
{ 
	Name = GNamePool.AddName(InName);
    //OutputDebugStringA("1\n");

    //if (Outer == nullptr)
    //    return;
    //OutputDebugStringA("2g\n");

    //uint32 Number = GUniqueNamePool.AddNameAndCount(Outer->GetUUID(), Name.GetDisplayIndex());
    //Name.SetNumber(Number);
}

uint64 UObject::GetAllocatedBytes() const 
{ 
	return AllocatedBytes; 
}

uint32 UObject::GetAllocatedCount() const 
{ 
	return AllocatedCounts; 
}

bool UObject::IsA(UClass *TargetClass) const
{
    UClass *CurrentClass = GetClass();

    while (CurrentClass != nullptr)
    {
        if (CurrentClass == TargetClass)
        {
            return true;
        }
        CurrentClass = CurrentClass->SuperClass;
    }
    return false;
}

//해당 클래스와 정확히 일치하는지 파악하는 함수
bool UObject::IsExactly(UClass* InClass) const
{
    UClass *CurrentClass = GetClass();

    if (CurrentClass == InClass)
    {
        return true;
    }
    
    return false;
}

void UObject::SetOuter(UObject *InObject)
{
	Outer = InObject;
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

