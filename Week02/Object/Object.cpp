#include "CoreTypes.h"
#include "Object.h"
#include "EngineStatics.h"

uint32 UEngineStatics::NextUUID = 0;
TArray<UObject *> GUObjectArray;

UObject::UObject(const FString &InString) : Name(InString), Outer(nullptr)
{
    UUID = UEngineStatics::GenUUID();

    GUObjectArray.push_back(this);
    InternalIndex = static_cast<uint32>(GUObjectArray.size()) - 1;
}

UObject::~UObject()
{
    auto it = std::find(GUObjectArray.begin(), GUObjectArray.end(), this);
    if (it != GUObjectArray.end())
    {
        GUObjectArray.erase(it);
    }
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

UClass *UObject::StaticClass()
{
    // UObject는 최상위 부모이며, 직접 생성하지 않으므로 생성 함수는 nullptr
    static UClass s_Class("UObject", nullptr, nullptr);
    return &s_Class;
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

