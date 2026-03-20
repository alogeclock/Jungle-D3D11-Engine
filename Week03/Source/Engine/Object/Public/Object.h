#pragma once

#include "CoreTypes.h"
class UObject;

using ConstructFn = UObject * (*)();


class UClass
{
public:
	FString     ClassName;
	UClass* SuperClass;
	uint32      ClassSize;
	ConstructFn Constructor;

	UClass(FString InClassName, UClass* InSuperClass, uint32 InClassSize, ConstructFn InConstructor) : ClassName(InClassName), SuperClass(InSuperClass), ClassSize(InClassSize), Constructor(InConstructor) {}
	const char* GetName() { return ClassName.c_str(); }
};

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

#define DECLARE_OBJECT(CurrentType, SuperType)                           \
public:\
	static UClass* StaticClass()\
{\
	static UClass s_Class(#CurrentType, SuperType::StaticClass(), sizeof(CurrentType), Constructor);\
	return &s_Class;\
}\
virtual UClass* GetClass() const override\
{\
		return CurrentType::StaticClass(); \
}\
\
static UObject* Constructor()\
{\
		return new CurrentType(#CurrentType); \
}

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

	const FString& GetName() const;
	void SetName(const FString& InName);

	virtual class UWorld* GetWorld() const;

	UObject* GetOuter() const { return Outer; }
	void SetOuter(UObject* InObject);

	void AddMemoryUsage(uint64 InBytes, uint32 InCount = 1);
	void RemoveMemoryUsage(uint64 InBytes, uint32 InCount = 1);

	uint32 GetUUID() const { return UUID; }
	uint64 GetAllocatedBytes() const;
	uint32 GetAllocatedCount() const;

	template <typename T> T* CreateDefaultSubobject();
	bool            IsA(UClass* TargetClass) const;

private:
	uint32 UUID = -1;
	uint32 InternalIndex = -1;
	FString Name;
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