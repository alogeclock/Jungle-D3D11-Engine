#pragma once

#include "CoreTypes.h"
#include "Source/Core/Public/FName.h"
#include "Source/Core/Public/UClass.h"

class FObjectFactory
{
public:
	static UObject* ConstructObject(UClass* TargetClass)
	{
		if (TargetClass != nullptr && TargetClass->Constructor != nullptr)
		{
			return TargetClass->Constructor();
		}
		return nullptr;
	}
};



class UObject
{
public:
	static UClass* StaticClass()
	{
		static UClass s_Class("UObject", nullptr, sizeof(UObject), Constructor);
		return &s_Class;
	}
	virtual UClass* GetClass() const
	{
		return UObject::StaticClass();
	}
	static UObject* Constructor()
	{
		return new UObject("UObject");
	}

public:
	explicit UObject(const FString& InString);
	virtual ~UObject();

	const FName& GetName() const;
	void SetName(const FString& InName);

	virtual class UWorld* GetWorld() const;

	UObject* GetOuter() const { return Outer; }
	void SetOuter(UObject* InObject);

	void AddMemoryUsage(uint64 InBytes, uint32 InCount = 1);
	void RemoveMemoryUsage(uint64 InBytes, uint32 InCount = 1);

	uint32 GetUUID() const { return UUID; }
	uint64 GetAllocatedBytes() const;
	uint32 GetAllocatedCount() const;
    uint32 GetInternalIndex() const { return InternalIndex; }

	bool IsValid();

	template <typename T> T* CreateDefaultSubobject();
	bool            IsA(UClass* TargetClass) const;
	bool            IsExactly(UClass* TargetClass) const;

private:
	uint32 UUID = -1;
	uint32 InternalIndex = -1;
	FName Name;
	UObject* Outer;

	uint64 AllocatedBytes = 0;
	uint32 AllocatedCounts = 0;
};

extern TArray<UObject*> GUObjectArray;

template <typename T> inline T* UObject::CreateDefaultSubobject()
{
	T* NewSubobject = new T("SceneComponent");

	NewSubobject->SetOuter(this);

	return NewSubobject;
}

template <typename T> inline T* Cast(UObject* Src)
{
	// Src가 유효하고, T의 클래스 타입과 일치하거나 자식 클래스라면
	if (Src != nullptr && Src->IsA(T::StaticClass()))
	{
		// 안전하게 static_cast로 다운캐스팅
		return static_cast<T*>(Src);
	}
	return nullptr;
}