#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "Core/CoreTypes.h"

namespace json { class JSON; }

struct FArrayAccessor
{
	uint32	(*Num)(const void* ArrayPtr);
	void*	(*GetAt)(void* ArrayPtr, uint32 Index);
	void	(*AddDefault)(void* ArrayPtr);
	void	(*RemoveAt)(void* ArrayPtr, uint32 Index);
	void	(*Clear)(void* ArrayPtr);
	void	(*Assign)(void* DstArr, const void* SrcArr);
};

template <typename T>
inline FArrayAccessor* GetTArrayAccessor()
{
	static FArrayAccessor s = {
		+[](const void* A) -> uint32 { return (uint32)static_cast<const TArray<T>*>(A)->size(); },
		+[](void* A, uint32 i) -> void* { return &(*static_cast<TArray<T>*>(A))[i]; },
		+[](void* A) { static_cast<TArray<T>*>(A)->emplace_back(); },
		+[](void* A, uint32 i) { auto& V = *static_cast<TArray<T>*>(A); V.erase(V.begin() + i); },
		+[](void* A) { static_cast<TArray<T>*>(A)->clear(); },
		+[](void* D, const void* S) { *static_cast<TArray<T>*>(D) = *static_cast<const TArray<T>*>(S); },
	};
	return &s;
}

// 에디터에서 자동 위젯 매핑에 사용되는 프로퍼티 타입
enum class EPropertyType : uint8_t
{
	Bool,
	ByteBool,			// uint8을 bool처럼 사용 (std::vector<bool> 회피용)
	Int,
	Float,
	Vec3,
	Vec4,
	Rotator,			// FRotator (Pitch, Yaw, Roll)
	String,
	Name,				// FName — 문자열 풀 기반 이름 (리소스 키 등)
	SceneComponentRef,	// Owner actor 내부 USceneComponent 참조
	Color4,				// FVector4 RGBA — ImGui::ColorEdit4 위젯
	StaticMeshRef,		// UStaticMesh* 에셋 레퍼런스 (드롭다운 선택)
	SkeletalMeshRef,	// USkeletalMesh* 에셋 레퍼런스 (드롭다운 선택)
	MaterialSlot,		// FMaterialSlot — 머티리얼 경로
	Enum,
	Struct,				// 자기기술 구조체 — StructFunc로 Children 생성
	Script,
	Array,
};

enum EPropertyFlags : uint32 {
	CPF_None			= 0,
	CPF_Edit			= 1 << 1,			// The property can be edited in the Details Panel.
	CPF_FixedSize		= 1 << 2,			// Property slots cannot be added or removed.
	CPF_Transient		= 1 << 3,			// The property is not saved to disk (ignored during serialization).
	CPF_Config			= 1 << 4,			// TODO: The property can be loaded from and saved to .ini configuration files.
};

// 머티리얼 슬롯: 경로를 하나의 단위로 관리
struct FMaterialSlot
{
	std::string Path;
};

class FProperty;

// 구조체 자기기술 함수: 구조체 포인터로부터 하위 프로퍼티를 생성
using FStructPropertyFunc = void(*)(void* StructPtr, std::vector<FProperty>& OutProps);

// 컴포넌트가 노출하는 편집 가능한 프로퍼티 디스크립터
class FProperty
{
public:
	// 의도적으로 사용자 정의 dtor 없음. FProperty 는 값으로 자유롭게 복사된다
	// (UClass::GetAllProperties -> editor TArray<FProperty>). Inner 는 UClass 가
	// 소유한 원본 1개만 살아있고, 모든 복사본이 같은 포인터를 공유한다. dtor 에서
	// delete Inner 하면 사본 destructor 한 번에 원본이 dangling 되어 다음 프레임
	// 에 crash. 소유권은 UClass::~UClass 가 일괄 처리한다 (process 종료 시점).

	FString		  Name;
	EPropertyType Type			= EPropertyType::Bool;
	FString		  Category;      // 에디터 카테고리 (같은 문자열끼리 그룹화)
	void*         ValuePtr		= nullptr;

	// float 범위 힌트 (DragFloat 등에서 사용)
	float Min   = 0.0f;
	float Max   = 0.0f;
	float Speed = 0.1f;

	// Enum Metadata
	const char** EnumNames = nullptr;
	uint32		 EnumCount = 0;
	uint32		 EnumSize  = sizeof(int32); // underlying type 크기 (uint8 enum은 1)

	// Struct Metadata
	FStructPropertyFunc StructFunc = nullptr;

	uint32		  PropertyFlag = EPropertyFlags::CPF_None;
	uint32		  ElementSize = 0;		// Container size for Array type
	uint32		  Offset_Internal = 0;

	FProperty*		Inner	 = nullptr; // element descriptor; heap-owned by parent
	FArrayAccessor* Accessor = nullptr; // Static - not owned

	// JSON 직렬화 — FSceneSaveManager 등 외부 직렬자가 호출.
	// 헤더에 SimpleJSON 의존을 들이지 않기 위해 본문은 PropertyTypes.cpp 에 둔다.
	json::JSON Serialize() const;
	void	   Deserialize(json::JSON& Value);
};
