#pragma once

#include "Core/CoreTypes.h"
#include "Core/PropertyTypes.h"

#include <cstring>
#include <unordered_set>

class UObject;
class FArchive;

enum EClassFlags : uint32
{
	CF_None      = 0,
	CF_Actor     = 1 << 0,
	CF_Component = 1 << 1,
	CF_Camera    = 1 << 2,
	CF_HiddenInComponentList = 1 << 3,
};

class UClass
{
public:
	UClass(const char* InName, UClass* InSuperClass, size_t InSize, uint32 InFlags = CF_None)
		: Name(InName), SuperClass(InSuperClass), Size(InSize), ClassFlags(InFlags)
	{}

	~UClass()
	{
		for (uint32 i = 0; i < Properties.size(); i++)
		{
			if (Properties[i])
			{
				// Inner 는 PROPERTY_ARRAY 매크로가 heap 에 할당해 부모에만 1개 보관.
				// FProperty 자체에는 dtor 가 없으므로 (값 복사 안전성 때문) 여기서 명시 해제.
				if (Properties[i]->Inner)
				{
					delete Properties[i]->Inner;
					Properties[i]->Inner = nullptr;
				}
				delete Properties[i];
				Properties[i] = nullptr;
			}
		}
		Properties.clear();
	}

	//void		 Serialize(FArchive& Ar);
	const char*  GetName()       const { return Name; }
	UClass*      GetSuperClass() const { return SuperClass; }
	size_t       GetSize()       const { return Size; }
	uint32       GetClassFlags() const { return ClassFlags; }
	void         AddClassFlags(uint32 Flags) { ClassFlags |= Flags; }
	const TArray<FProperty*>& GetProperties() const { return Properties; }
	FProperty*   GetProperty(uint32 Index) const { return Index < Properties.size() ? Properties[Index] : nullptr; }
	void		 AddProperty(FProperty* InProperty) { if (InProperty) Properties.push_back(InProperty); }

	// Hide the property from both the editor AND the serializing process
	void		 HideInheritedProperty(FString InName);
	bool		 IsPropertyHidden(FString InName) const;

	// 베이스 → 파생 순서로 모든 프로퍼티 템플릿을 OutProps 에 복사.
	// 기존 GetEditableProperties 구현이 Super 를 먼저 호출하던 순서와 동일하다.
	// 복사본을 돌려주므로 호출자가 ValuePtr 을 인스턴스 주소로 패치해도 템플릿은 안전.
	void GetAllProperties(TArray<FProperty>& OutProps) const
	{
		if (SuperClass) SuperClass->GetAllProperties(OutProps);
		for (const FProperty* P : Properties)
		{
			if (P) OutProps.push_back(*P);
		}
	}

	void GetEditableProperties(TArray<FProperty>& OutProps) const;
	void GetNonTransientProperties(TArray<FProperty>& OutProps) const;

	// 이름으로 프로퍼티 룩업. 자기 클래스 → 베이스 순서로 검색.
	const FProperty* FindPropertyByName(const char* InName) const
	{
		if (!InName) return nullptr;
		for (const FProperty* P : Properties)
		{
			if (P && std::strcmp(P->Name.c_str(), InName) == 0) return P;
		}
		return SuperClass ? SuperClass->FindPropertyByName(InName) : nullptr;
	}

	bool IsA(const UClass* Other) const
	{
		for (const UClass* C = this; C; C = C->SuperClass)
		{
			if (C == Other)
				return true;
		}
		return false;
	}

	bool HasAnyClassFlags(uint32 Flags) const
	{
		return (ClassFlags & Flags) != 0;
	}

	// --- Global class registry ---
	static TArray<UClass*>& GetAllClasses()
	{
		static TArray<UClass*> Registry;
		return Registry;
	}

	// 이름으로 등록된 클래스 룩업. 못 찾으면 nullptr.
	// 직렬화/설정 파일에서 클래스를 string으로 지정할 때 사용.
	static UClass* FindByName(const char* InName)
	{
		if (!InName) return nullptr;
		for (UClass* C : GetAllClasses())
		{
			if (C && C->GetName() && std::strcmp(C->GetName(), InName) == 0)
				return C;
		}
		return nullptr;
	}

private:
	void GetEditablePropertiesFor(TArray<FProperty>& OutProps, const UClass* TargetClass) const;
	void GetNonTransientPropertiesFor(TArray<FProperty>& OutProps, const UClass* TargetClass) const;

private:
	const char* Name        = nullptr;
	UClass*     SuperClass  = nullptr;
	size_t      Size        = 0;
	uint32      ClassFlags  = CF_None;

	TArray<FProperty*> Properties;
	std::unordered_set<FString> HiddenProperties;
};

// static initializer 에서 UClass를 전역 레지스트리에 등록
struct FClassRegistrar
{
	FClassRegistrar(UClass* InClass)
	{
		UClass::GetAllClasses().push_back(InClass);
	}
};
