#pragma once

#include "CoreTypes.h"
#include "Source/Engine/Public/Rendering/Renderer.h"

class UObject;

using ConstructFn = UObject* (*)();

class UClass
{
  public:
    FString     ClassName;
    UClass     *SuperClass;
    ConstructFn Constructor;

    UClass(const FString &InName, UClass *InSuper, ConstructFn InConstructor) 
        : ClassName(InName), SuperClass(InSuper), Constructor(InConstructor) {}
    const char *GetName() { return ClassName.c_str();}
};

class FObjectFactory
{
  public:
    static UObject *ConstructObject(UClass *TargetClass)
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
	explicit UObject(const FString& InString);
  virtual ~UObject();

	const FString& GetName() const;
	void SetName(const FString& InName);

	virtual class UWorld *GetWorld() const;

	UObject *GetOuter() const { return Outer; }
    void SetOuter(UObject *InObject);

	void AddMemoryUsage(uint64 InBytes, uint32 InCount = 1);
	void RemoveMemoryUsage(uint64 InBytes, uint32 InCount = 1);

    uint32 GetUUID() const { return UUID; }
	uint64 GetAllocatedBytes() const;
	uint32 GetAllocatedCount() const;

	template <typename T> T *CreateDefaultSubobject();
    
	static UClass  *StaticClass();
    virtual UClass *GetClass() const { return StaticClass(); }
    bool            IsA(UClass *TargetClass) const;

private:
	uint32 UUID = -1;
	uint32 InternalIndex = -1;
	FString Name;
    UObject *Outer;

	uint64 AllocatedBytes = 0;
	uint32 AllocatedCounts = 0;
};

extern TArray<UObject *> GUObjectArray;

template <typename T> inline T *UObject::CreateDefaultSubobject()
{
    T *NewSubobject = new T("SceneComponent");

	NewSubobject->SetOuter(this);

    return NewSubobject;
}

template <typename T> inline T *Cast(UObject *Src)
{
    // Src가 유효하고, T의 클래스 타입과 일치하거나 자식 클래스라면
    if (Src != nullptr && Src->IsA(T::StaticClass()))
    {
        // 안전하게 static_cast로 다운캐스팅
        return static_cast<T *>(Src);
    }
    return nullptr;
}