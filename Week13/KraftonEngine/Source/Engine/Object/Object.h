#pragma once

#include "Profiling/MemoryStats.h"
#include "Object/FName.h"
#include "Core/Singleton.h"
#include "Core/PropertyTypes.h"
#include "Object/UClass.h"

class FArchive;

// ---------------------------------------------------------------------------
// RTTI Macros
// ---------------------------------------------------------------------------

#define DECLARE_CLASS(ClassName, ParentClass)                               \
    using Super = ParentClass;                                             \
    static UClass StaticClassInstance;                                      \
    static FClassRegistrar s_Registrar;                                    \
    static UClass* StaticClass() { return &StaticClassInstance; }           \
    UClass* GetClass() const override { return StaticClass(); }				\
	friend struct ClassName##_PropertyRegistrar;

#define DEFINE_CLASS_WITH_FLAGS(ClassName, ParentClass, FlagsValue)         \
    UClass ClassName::StaticClassInstance(                                  \
        #ClassName,                                                        \
        &ParentClass::StaticClassInstance,                                  \
        sizeof(ClassName),                                                 \
        FlagsValue                                                         \
    );                                                                     \
    FClassRegistrar ClassName::s_Registrar(&ClassName::StaticClassInstance);

#define DEFINE_CLASS(ClassName, ParentClass)                                \
    DEFINE_CLASS_WITH_FLAGS(ClassName, ParentClass, CF_None)

// ---------------------------------------------------------------------------
// Property registration — UPROPERTY-style 매크로.
// 
//     BEGIN_CLASS_PROPERTIES(UCarMovementComponent)
//         PROPERTY_FLOAT(MaxSpeed, "Max Speed", "Movement", 0.0f, 200.0f, 0.5f, CPF_Edit)
//         PROPERTY_BOOL(bUseRaycastSuspension, "Use Raycast Suspension", "Suspension", CPF_Edit)
//     END_CLASS_PROPERTIES(UCarMovementComponent)
//
// 인스턴스 바인딩(ValuePtr = this+Offset)은 UObject::GetEditableProperties 가 수행.
// ---------------------------------------------------------------------------

#define BEGIN_CLASS_PROPERTIES(ClassName)                                   \
	struct ClassName##_PropertyRegistrar {                                  \
		ClassName##_PropertyRegistrar() {                                   \
			using ThisClass = ClassName;                                    \
			UClass* Cls = ClassName::StaticClass();                         \
			(void)Cls;

#define END_CLASS_PROPERTIES(ClassName)                                     \
		}                                                                   \
	};                                                                      \
	static ClassName##_PropertyRegistrar s_##ClassName##_PropertyReg;

#define HIDE_PROPERTY(PropertyName) \
	Cls->HideInheritedProperty(PropertyName);

// 모든 PROPERTY_* 매크로의 공통 구현.
// ExtraSetup 은 type-specific 필드를 채우는 쉼표 표현식(예: (P->Min = X, P->Max = Y)).
// 추가 필드가 없으면 (void)0 전달.
#define KE_REGISTER_PROPERTY_IMPL(MemberName, InName, InType, InCategory, InFlags, ExtraSetup) \
	{                                                                                                 \
		FProperty* P = new FProperty();                                                               \
		P->Name            = (InName);                                                                \
		P->Type            = (InType);                                                                \
		P->Category        = (InCategory);                                                            \
		P->PropertyFlag    = (InFlags);                                                               \
		P->Offset_Internal = static_cast<uint32>(offsetof(ThisClass, MemberName));                    \
		P->ElementSize     = static_cast<uint32>(sizeof(((ThisClass*)0)->MemberName));                \
		ExtraSetup;                                                                                   \
		Cls->AddProperty(P);                                                                          \
	}

#define KE_REGISTER_PROPERTY_OFFSET_IMPL(InName, InType, InCategory, InOffset, InSize, InFlags, ExtraSetup) \
	{                                                                                                 \
		FProperty* P = new FProperty();                                                               \
		P->Name            = (InName);                                                                \
		P->Type            = (InType);                                                                \
		P->Category        = (InCategory);                                                            \
		P->PropertyFlag    = (InFlags);                                                               \
		P->Offset_Internal = static_cast<uint32>(InOffset);                                           \
		P->ElementSize     = static_cast<uint32>(InSize);                                             \
		ExtraSetup;                                                                                   \
		Cls->AddProperty(P);                                                                          \
	}

#define PROPERTY_BOOL(MemberName, InName, InCategory, InFlags) \
	KE_REGISTER_PROPERTY_IMPL(MemberName, InName, EPropertyType::Bool,   InCategory, InFlags, (void)0)
#define PROPERTY_INT(MemberName, InName, InCategory, InFlags) \
	KE_REGISTER_PROPERTY_IMPL(MemberName, InName, EPropertyType::Int,    InCategory, InFlags, (void)0)
#define PROPERTY_VEC3(MemberName, InName, InCategory, InFlags) \
	KE_REGISTER_PROPERTY_IMPL(MemberName, InName, EPropertyType::Vec3,   InCategory, InFlags, (void)0)
#define PROPERTY_STRING(MemberName, InName, InCategory, InFlags) \
	KE_REGISTER_PROPERTY_IMPL(MemberName, InName, EPropertyType::String, InCategory, InFlags, (void)0)

#define PROPERTY_FLOAT(MemberName, InName, InCategory, InMin, InMax, InSpeed, InFlags) \
	KE_REGISTER_PROPERTY_IMPL(MemberName, InName, EPropertyType::Float, InCategory, InFlags, \
		(P->Min = (InMin), P->Max = (InMax), P->Speed = (InSpeed)))

// Enum 멤버 등록. EnumNamesArr/EnumCountVal/EnumSizeVal 은 호출자가 제공.
//   예: PROPERTY_ENUM(CollisionEnabled, "Collision Enabled", "Collision",
//                     GCollisionEnabledNames, (uint32)ECollisionEnabled::COUNT,
//                     sizeof(ECollisionEnabled), CPF_Edit)
#define PROPERTY_ENUM(MemberName, InName, InCategory, EnumNamesArr, EnumCountVal, EnumSizeVal, InFlags) \
	KE_REGISTER_PROPERTY_IMPL(MemberName, InName, EPropertyType::Enum, InCategory, InFlags, \
		(P->EnumNames = (EnumNamesArr), P->EnumCount = (EnumCountVal), P->EnumSize = (EnumSizeVal)))

// Struct 멤버 등록. StructFuncPtr 는 자식 프로퍼티 디스크립터를 만드는 콜백.
//   예: PROPERTY_STRUCT(ResponseContainer, "Collision Responses", "Collision",
//                       &FCollisionResponseContainer::DescribeProperties, CPF_Edit)
#define PROPERTY_STRUCT(MemberName, InName, InCategory, StructFuncPtr, InFlags) \
	KE_REGISTER_PROPERTY_IMPL(MemberName, InName, EPropertyType::Struct, InCategory, InFlags, \
		P->StructFunc = (StructFuncPtr))

// TArray 멤버 등록
#define PROPERTY_ARRAY(MemberName, InName, InCategory, InFlags, ElementType, InnerType, InnerSetup) \
    {                                                                                               \
        FProperty* P = new FProperty();                                                             \
        P->Name            = (InName);                                                              \
        P->Type            = EPropertyType::Array;                                                  \
        P->Category        = (InCategory);                                                          \
        P->PropertyFlag    = (InFlags);                                                             \
        P->Offset_Internal = static_cast<uint32>(offsetof(ThisClass, MemberName));                  \
        P->ElementSize     = static_cast<uint32>(sizeof(((ThisClass*)0)->MemberName));              \
        P->Accessor        = GetTArrayAccessor<ElementType>();                                      \
        FProperty* Inner   = new FProperty();                                                       \
        Inner->Name        = "Element";                                                             \
        Inner->Type        = (InnerType);                                                           \
        Inner->Category    = (InCategory);                                                          \
        Inner->ElementSize = static_cast<uint32>(sizeof(ElementType));                              \
        /* Inner->Offset_Internal intentionally unused since accessor resolves element address */   \
        InnerSetup;                                                                                 \
        P->Inner = Inner;                                                                           \
        Cls->AddProperty(P);                                                                        \
    }


// 일반화: 명시적 EPropertyType 으로 등록. 위 매크로가 못 잡는 케이스용.
#define REGISTER_PROPERTY(MemberName, InName, InType, InCategory, InFlags) \
	KE_REGISTER_PROPERTY_IMPL(MemberName, InName, InType, InCategory, InFlags, (void)0)

// 명시적 offset 버전. 중첩 멤버처럼 offsetof(ThisClass, MemberName) 으로 표현할 수 없는
// 필드를 등록할 때 사용한다.
#define REGISTER_PROPERTY_OFFSET(InName, InType, InCategory, InOffset, InSize, InFlags) \
	KE_REGISTER_PROPERTY_OFFSET_IMPL(InName, InType, InCategory, InOffset, InSize, InFlags, (void)0)

#define PROPERTY_FLOAT_OFFSET(InName, InCategory, InOffset, InMin, InMax, InSpeed, InFlags) \
	KE_REGISTER_PROPERTY_OFFSET_IMPL(InName, EPropertyType::Float, InCategory, InOffset, sizeof(float), InFlags, \
		(P->Min = (InMin), P->Max = (InMax), P->Speed = (InSpeed)))

#define PROPERTY_BOOL_OFFSET(InName, InCategory, InOffset, InFlags) \
	KE_REGISTER_PROPERTY_OFFSET_IMPL(InName, EPropertyType::Bool, InCategory, InOffset, sizeof(bool), InFlags, (void)0)

#define PROPERTY_ENUM_OFFSET(InName, InCategory, InOffset, InSize, EnumNamesArr, EnumCountVal, EnumSizeVal, InFlags) \
	KE_REGISTER_PROPERTY_OFFSET_IMPL(InName, EPropertyType::Enum, InCategory, InOffset, InSize, InFlags, \
		(P->EnumNames = (EnumNamesArr), P->EnumCount = (EnumCountVal), P->EnumSize = (EnumSizeVal)))

#define PROPERTY_STRUCT_OFFSET(InName, InCategory, InOffset, InSize, StructFuncPtr, InFlags) \
	KE_REGISTER_PROPERTY_OFFSET_IMPL(InName, EPropertyType::Struct, InCategory, InOffset, InSize, InFlags, \
		P->StructFunc = (StructFuncPtr))

// ---------------------------------------------------------------------------

// Forward — IsValid 의 실제 정의는 GUObjectSet 선언 뒤. UObject::GetTypedOuter 가
// non-dependent name lookup 으로 IsValid 를 찾을 수 있게 미리 알려둠.
class UObject;
inline bool IsValid(const UObject* Object);

class UObject
{
public:
	UObject();
	virtual ~UObject();

	uint32 GetUUID() const { return UUID; }
	uint32 GetInternalIndex() const { return InternalIndex; }
	void SetUUID(uint32 InUUID) { UUID = InUUID; }
	void SetInternalIndex(uint32 InIndex) { InternalIndex = InIndex; }

	// Outer — 객체의 논리적 스코프 (소유 의미 아님). 직렬화 제외.
	UObject* GetOuter() const { return Outer; }
	void SetOuter(UObject* InOuter) { Outer = InOuter; }

	// Outer 체인을 따라 첫 번째 T를 찾는다 (UE의 GetTypedOuter<T>와 동일 시맨틱).
	// PendingKill 처리 도중 World 가 actor 보다 먼저 delete 되면 component 의
	// DestroyRenderState 가 Owner->GetWorld → GetTypedOuter<UWorld> 경로를 타다가
	// freed Outer 를 deref 해 crash 났음. 매 iteration 에서 IsValid 로 살아있는 UObject
	// 만 따라가도록 가드.
	template<typename T>
	T* GetTypedOuter() const
	{
		for (UObject* O = Outer; IsValid(O); O = O->Outer)
		{
			if (T* Hit = Cast<T>(O))
			{
				return Hit;
			}
		}
		return nullptr;
	}

	virtual UObject* Duplicate(UObject* NewOuter = nullptr) const;
	virtual void Serialize(FArchive& Ar);
	virtual void PostDuplicate() {}

	virtual void GetAllProperties(TArray<FProperty>& OutProps);
	virtual void GetEditableProperties(TArray<FProperty>& OutProps);
	virtual void PostEditProperty(const char* PropertyName);
	virtual void GetNonTransientProperties(TArray<FProperty>& OutProps);

	static void* operator new(size_t Size)
	{
		void* Ptr = std::malloc(Size);
		if (Ptr)
		{
			MemoryStats::OnAllocated(static_cast<uint32>(Size));
		}
		return Ptr;
	}

	static void operator delete(void* Ptr, size_t Size)
	{
		if (Ptr)
		{
			MemoryStats::OnDeallocated(static_cast<uint32>(Size));
			std::free(Ptr);
		}
	}

	// FName
	FName GetFName() const { return ObjectName; }
	FString GetName() const { return ObjectName.ToString(); }
	void SetFName(const FName& InName) { ObjectName = InName; }

	// RTTI
	virtual UClass* GetClass() const { return StaticClass(); }

	template<typename T>
	bool IsA() const { return GetClass()->IsA(T::StaticClass()); }

	static UClass StaticClassInstance;
	static UClass* StaticClass() { return &StaticClassInstance; }

protected:
	FName ObjectName;

private:
	uint32 UUID;
	uint32 InternalIndex;
	UObject* Outer = nullptr;
};

extern TArray<UObject*> GUObjectArray;
// 살아있는 UObject 포인터를 O(1) 로 조회하기 위한 set. UObject ctor/dtor 가 자동 유지.
// dangling pointer 도 hash 만 계산하므로(deref 없음) 안전.
extern TSet<UObject*> GUObjectSet;

// 포인터가 현재 살아있는 UObject 를 가리키는지 확인. dangling/freed 포인터가 들어와도
// 해시 테이블 조회만 하므로 deref 안 함 — 안전.
inline bool IsValid(const UObject* Object)
{
	return Object && GUObjectSet.find(const_cast<UObject*>(Object)) != GUObjectSet.end();
}

inline bool IsAliveObject(const UObject* Object)
{
	return IsValid(Object);
}

class UObjectManager : public TSingleton<UObjectManager>
{
	friend class TSingleton<UObjectManager>;

public:
	template<typename T>
	T* CreateObject(UObject* InOuter = nullptr)
	{
		static_assert(std::is_base_of<UObject, T>::value, "T must derive from UObject");
		T* Obj = new T();
		Obj->SetOuter(InOuter);

		const char* ClassName = T::StaticClass()->GetName();
		uint32& Counter = NameCounters[ClassName];
		FString Name = FString(ClassName) + "_" + std::to_string(Counter++);
		Obj->SetFName(FName(Name));

		return Obj;
	}

	// 즉시 destroy. dangling 포인터 위험은 octree / spatial partition / UObject 추적 set
	// 측에서 IsValid 가드로 처리하므로 별도 deferred 큐 (PendingKill) 는 두지 않는다.
	void DestroyObject(UObject* Obj)
	{
		if (!Obj) return;
		delete Obj;
	}

private:
	TMap<FString, uint32> NameCounters;

public:
	UObject* FindByUUID(uint32 InUUID)
	{
		for (auto* Obj : GUObjectArray)
			if (Obj && Obj->GetUUID() == InUUID)
				return Obj;
		return nullptr;
	}

	UObject* FindByIndex(uint32 Index)
	{
		if (Index >= GUObjectArray.size()) return nullptr;
		return GUObjectArray[Index];
	}
};

template<typename T>
T* Cast(UObject* Obj)
{
	return (Obj && Obj->IsA<T>()) ? static_cast<T*>(Obj) : nullptr;
}

template<typename T>
const T* Cast(const UObject* Obj)
{
	return (Obj && Obj->IsA<T>()) ? static_cast<const T*>(Obj) : nullptr;
}
