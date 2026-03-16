#pragma once

#include "CoreTypes.h"

class UObject
{
public:
	UObject();
	explicit UObject(const FString& InString);
	virtual ~UObject() = default;

	const FString& GetName() const;
	void SetName(const FString& InName);

	const UObject *GetOuter() const { return Outer; }
    void SetOuter(UObject *InObject);

	virtual class UWorld *GetWorld() const;

	void AddMemoryUsage(uint64 InBytes, uint32 InCount = 1);
	void RemoveMemoryUsage(uint64 InBytes, uint32 InCount = 1);

	uint64 GetAllocatedBytes() const;
	uint32 GetAllocatedCount() const;

private:
	uint32 UUID = -1;
	uint32 InternalIndex = -1;
	FString Name;
    UObject *Outer;

	uint64 AllocatedBytes = 0;
	uint32 AllocatedCounts = 0;
};

extern TArray<UObject *> GUObjectArray;