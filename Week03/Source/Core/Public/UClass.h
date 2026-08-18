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

    TArray<FProperty> &GetProperties() { return Properties; }
    void               AddProperty(const FProperty &property) { Properties.push_back(property); }
    const char        *GetName() { return ClassName.c_str(); }
};

#define DECLARE_OBJECT(CurrentType, SuperType)                                                                                                                 \
  public:                                                                                                                                                      \
    virtual UClass *GetClass() const override { return CurrentType::StaticClass(); }                                                                           \
                                                                                                                                                               \
  private:                                                                                                                                                     \
    static UObject       *Constructor() { return new CurrentType(#CurrentType); }                                                                              \
    inline static UClass *_StaticUClass = nullptr;                                                                                                             \
                                                                                                                                                               \
  public:                                                                                                                                                      \
    static UClass *StaticClass()                                                                                                                               \
    {                                                                                                                                                          \
        static UClass *s_Class = []() -> UClass *                                                                                                              \
        {                                                                                                                                                      \
            UClass            *cls = new UClass(#CurrentType, SuperType::StaticClass(), sizeof(CurrentType), &CurrentType::Constructor);                       \
            UClass            *SuperClass = SuperType::StaticClass();                                                                                          \
            TArray<FProperty> &Properties = SuperClass->GetProperties();                                                                                       \
            int SuperPropertyCount = static_cast<int>(Properties.size());                                                                                                        \
            for (int i = 0; i < SuperPropertyCount; i++)                                                                                                       \
            {                                                                                                                                                  \
                if (Properties[i].bIsPublic)                                                                                                                   \
                    cls->AddProperty(Properties[i]);                                                                                                           \
            }                                                                                                                                                  \
            return cls;                                                                                                                                        \
        }();                                                                                                                                                   \
        return s_Class;                                                                                                                                        \
    }

#define DECLARE_OBJECT_START(CurrentType, SuperType)                                                                                                           \
  public:                                                                                                                                                      \
    virtual UClass *GetClass() const override { return CurrentType::StaticClass(); }                                                                           \
                                                                                                                                                               \
  private:                                                                                                                                                     \
    static UObject       *Constructor() { return new CurrentType(#CurrentType); }                                                                              \
    inline static UClass *_StaticUClass = nullptr;                                                                                                             \
                                                                                                                                                               \
  public:                                                                                                                                                      \
    static UClass *StaticClass()                                                                                                                               \
    {                                                                                                                                                          \
        static UClass *s_Class = []() -> UClass * {                                                                                                            \
            UClass* cls = new UClass(#CurrentType, SuperType::StaticClass(), sizeof(CurrentType), &CurrentType::Constructor);                                  \
            UClass* SuperClass = SuperType::StaticClass();                                                                                                     \
            TArray<FProperty>& Properties = SuperClass->GetProperties();                                                                                       \
            int SuperPropertyCount = static_cast<int>(Properties.size());                                                                                                        \
            for (int i = 0; i < SuperPropertyCount; i++)                                                                                                       \
            {                                                                                                                                                  \
                if(Properties[i].bIsPublic)                                                                                                                         \
                cls->AddProperty(Properties[i]);                                                                                                                    \
            }

// 내 클래스, 해당 멤버 변수 이름, 해당 멤버 자료형
// 자식에게도 상속되는 프로퍼티
#define PUBLIC_PROPERTY(ClassType, Member, TypeEnum) cls->AddProperty({#Member, EPropertyType::TypeEnum, offsetof(ClassType, Member), true});
// 내 클래스, 해당 멤버 변수 이름, 해당 멤버 자료형
// 자신만 소유하는 프로퍼티
#define PRIVATE_PROPERTY(ClassType, Member, TypeEnum) cls->AddProperty({#Member, EPropertyType::TypeEnum, offsetof(ClassType, Member), false});

#define DECLARE_END                                                                                                                                         \
    return cls;                                                                                                                                                \
    }                                                                                                                                                          \
    ();                                                                                                                                                        \
    return s_Class;                                                                                                                                            \
    }                                                                                                                                                          \
                                                                                                                                                               \
  private:
