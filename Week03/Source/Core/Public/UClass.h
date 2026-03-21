#pragma once
#include "CoreTypes.h"
#include "Source/Core/Public/FName.h"
#include "Source/Core/Public/Property.h"

class UObject;
using ConstructFn = UObject *(*)();

class UClass
{
    FString           ClassName;
    TArray<FProperty> Properties;
    uint32            ClassSize;

  public:
    UClass     *SuperClass;
    ConstructFn Constructor;
    UClass(FString InClassName, UClass *InSuperClass, uint32 InClassSize, ConstructFn InConstructor)
        : ClassName(InClassName), SuperClass(InSuperClass), ClassSize(InClassSize), Constructor(InConstructor)
    {
    }

    UClass(FString InClassName, UClass *InSuperClass, uint32 InClassSize, ConstructFn InConstructor, TArray<FProperty> &InProperties)
        : ClassName(InClassName), SuperClass(InSuperClass), ClassSize(InClassSize), Constructor(InConstructor), Properties(InProperties)
    {
    }
    TArray<FProperty> &GetProperties() { return Properties; }
    const char        *GetName() { return ClassName.c_str(); }
};

#define DECLARE_OBJECT(CurrentType, SuperType)                                                                                                                 \
  public:                                                                                                                                                      \
    static UClass *StaticClass()                                                                                                                               \
    {                                                                                                                                                          \
        static UClass s_Class(#CurrentType, SuperType::StaticClass(), sizeof(CurrentType), Constructor);                                                       \
        return &s_Class;                                                                                                                                       \
    }                                                                                                                                                          \
    virtual UClass *GetClass() const override { return CurrentType::StaticClass(); }                                                                           \
                                                                                                                                                               \
    static UObject *Constructor() { return new CurrentType(#CurrentType); }

#define DECLARE_UCLASS()                                                                                                                                       \
  private:                                                                                                                                                     \
    inline static UClass *_StaticUClass = nullptr;                                                                                                             \
                                                                                                                                                               \
  public:                                                                                                                                                      \
    static UClass *StaticClass()                                                                                                                               \
    {                                                                                                                                                          \
        if (_StaticUClass == nullptr)                                                                                                                          \
        {                                                                                                                                                      \
            TArray<FProperty> Properties;

#define REGISTER_PROPERTY(ClassType, Member, TypeEnum) Properties.push_back({ #Member, EPropertyType::TypeEnum, offsetof(ClassType, Member)});

#define END_DECLARE(CurrentType, SuperType)                                                                                                                    \
                                                                                                                                                               \
    UClass *s_Class = new UClass(#CurrentType, SuperType::StaticClass(), sizeof(CurrentType), Constructor, Properties);                                        \
    _StaticUClass = s_Class;                                                                                                                                   \
    }                                                                                                                                                          \
    return _StaticUClass;                                                                                                                                      \
    }                                                                                                                                                          \
                                                                                                                                                               \
    virtual UClass *GetClass() const override { return CurrentType::StaticClass(); }                                                                           \
    static UObject *Constructor() { return new CurrentType(#CurrentType); }                                                                                    \
  private:
