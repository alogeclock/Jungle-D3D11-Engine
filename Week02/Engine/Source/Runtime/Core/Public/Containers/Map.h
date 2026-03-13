#pragma once
#include <unordered_map>
#include "Array.h" // 앞서 만들었던 TArray가 필요합니다!

// CoreTypes.h에 정의되어 있다고 가정하는 기본 타입
typedef int32_t int32;

template <typename KeyType, typename ValueType>
class TMap
{
private:
    // 언리얼 TMap의 O(1) 검색 속도를 재현하기 위해 unordered_map 사용
    std::unordered_map<KeyType, ValueType> Data;

public:
    // ==========================================
    // 1. 데이터 추가 (Add)
    // ==========================================

    // 키와 값을 쌍으로 넣습니다. (이미 있는 키라면 값을 덮어씌웁니다)
    void Add(const KeyType& InKey, const ValueType& InValue)
    {
        Data[InKey] = InValue;
    }

    // ==========================================
    // 2. 데이터 검색 및 상태 확인 (Find & Status)
    // ==========================================

    // 요소의 개수
    int32 Num() const { return static_cast<int32>(Data.size()); }

    // 비어있는가?
    bool IsEmpty() const { return Data.empty(); }

    // 키가 존재하는지 확인 (값은 안 가져오고 존재 여부만!)
    bool Contains(const KeyType& InKey) const
    {
        return Data.find(InKey) != Data.end();
    }

    // 🔥 [언리얼 최적화의 핵심] 키로 값을 찾아서 '포인터'로 반환합니다.
    // 만약 못 찾으면 nullptr을 반환합니다. (Contains 후 []로 접근하는 것보다 2배 빠름!)
    ValueType* Find(const KeyType& InKey)
    {
        auto It = Data.find(InKey);
        if (It != Data.end())
        {
            return &(It->second); // 찾았으면 실제 데이터의 메모리 주소를 반환
        }
        return nullptr; // 없으면 null
    }

    const ValueType* Find(const KeyType& InKey) const
    {
        auto It = Data.find(InKey);
        if (It != Data.end())
        {
            return &(It->second);
        }
        return nullptr;
    }

    // ==========================================
    // 3. 데이터 삭제 (Remove)
    // ==========================================

    // 특정 키를 가진 데이터를 삭제
    int32 Remove(const KeyType& InKey)
    {
        return static_cast<int32>(Data.erase(InKey));
    }

    // 메모리까지 완전히 날려버림
    void Empty()
    {
        Data.clear();
        Data.rehash(0);
    }

    // ==========================================
    // 4. 데이터 접근 및 유틸리티 (Access & Utils)
    // ==========================================

    // 배열처럼 []로 접근 (주의: 키가 없으면 기본값으로 새로 만들어버림!)
    ValueType& operator[](const KeyType& InKey)
    {
        return Data[InKey];
    }

    // 🔥 실무에서 정말 유용한 기능! 키값들만 쏙 빼서 TArray로 만들어줍니다.
    TArray<KeyType> GenerateKeyArray() const
    {
        TArray<KeyType> Keys;
        for (const auto& Pair : Data)
        {
            Keys.Add(Pair.first);
        }
        return Keys;
    }

    // 값들만 쏙 빼서 TArray로 만들어줍니다.
    TArray<ValueType> GenerateValueArray() const
    {
        TArray<ValueType> Values;
        for (const auto& Pair : Data)
        {
            Values.Add(Pair.second);
        }
        return Values;
    }

    // ==========================================
    // 5. C++ 범위 기반 for 문 지원
    // ==========================================
    auto begin() { return Data.begin(); }
    auto end() { return Data.end(); }
    auto begin() const { return Data.begin(); }
    auto end() const { return Data.end(); }
};