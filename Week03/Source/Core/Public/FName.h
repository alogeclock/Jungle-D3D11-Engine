#pragma once
#include "CoreTypes.h"
#include <string>
#include <functional> // std::hash를 위해 추가

class FName
{
	friend class UniqueNamePool;
	friend struct std::hash<FName>;

	uint32 ComparisionIndex = 0;
	uint32 Number = 0;
	uint32 DisplayIndex = 0;
public:
	FName();
	FName(char* pStr);
	FName(const FString& Name);
	FName(uint32 InComparisionIndex, uint32 InDisplayIndex);

	bool operator==(const FName& Other) const;
	FString operator+(const FString& Other);

	int32 Compare(const FName& Other) const;

	FString ToString() const;

	void SetNumber(uint32 InNumber);

	uint32 GetDisplayIndex();
};

namespace std {
	// std::hash 구조체에 대한 템플릿 특수화
	template <>
	struct hash<FName>
	{
		size_t operator()(const FName& InName) const
		{
			// FName의 고유성을 결정하는 ComparisionIndex와 Number를 조합하여 해시값 생성
			size_t Hash1 = std::hash<uint32>()(InName.ComparisionIndex);
			size_t Hash2 = std::hash<uint32>()(InName.Number);

			// 비트 XOR 연산과 시프트 연산을 통해 두 해시값을 섞어줍니다.
			return Hash1 ^ (Hash2 << 1);
		}
	};
}

class FNamePool
{
private:
	//고유 이름과 키
	TMap<FString, uint32> NameToUniqueIndex;

	TArray<FString> UniqueNameArray;

	//같은 이름(소문자로 통일)이면 같은 인덱스
	TMap<FString, uint32> ComparsionPool;

public:
	FName AddName(const FString& InString);
	FString GetName(uint32 InNameIndex);
};

inline FString operator+(const FString& Other, FName Name)
{
	return Other + Name.ToString();
}

class UniqueNamePool
{
	//uuid, FName::comparisionIndex, Count, 
	TMap<uint32, TMap<uint32, uint32>> UniqueNameSet;



public:
	//Outer의 UUID참조
	//Outer의 의존된 Object의 이름 Set을 불러옴
	// 해당 이름의 카운트 증가
	//해당된 이름의 카운터를 증가
	uint32 AddNameAndCount(uint32 InOuterUUID, uint32 InName);
};

inline FNamePool GNamePool;
inline UniqueNamePool GUniqueNamePool;