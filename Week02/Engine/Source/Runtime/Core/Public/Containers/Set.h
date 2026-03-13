#pragma once
#include <unordered_set>
#include "Array.h" // 앞서 만들었던 TArray가 필요합니다!

// CoreTypes.h에 정의되어 있다고 가정하는 기본 타입
typedef int32_t int32;

template <typename T>
class TSet
{
private:
    // 언리얼 TSet의 O(1) 검색 속도를 재현하기 위해 unordered_set 사용
    std::unordered_set<T> Data;

public:
    // ==========================================
    // 1. 데이터 추가 (Add)
    // ==========================================

    // TSet은 중복을 허용하지 않습니다. 이미 있으면 아무 일도 안 일어납니다.
    void Add(const T& Element)
    {
        Data.insert(Element);
    }

    // ==========================================
    // 2. 정보 및 상태 확인 (Info & Status)
    // ==========================================

    // 요소의 개수
    int32 Num() const { return static_cast<int32>(Data.size()); }

    // 비어있는가?
    bool IsEmpty() const { return Data.empty(); }

    // 🔥 [핵심] 특정 요소를 가지고 있는가? (빛의 속도로 찾아냅니다)
    bool Contains(const T& Element) const
    {
        return Data.find(Element) != Data.end();
    }

    // ==========================================
    // 3. 데이터 삭제 (Remove & Empty)
    // ==========================================

    // 특정 요소를 삭제하고, 지운 개수를 반환 (지웠으면 1, 없었으면 0)
    int32 Remove(const T& Element)
    {
        return static_cast<int32>(Data.erase(Element));
    }

    // 메모리까지 완전히 날려버림
    void Empty()
    {
        Data.clear();
        Data.rehash(0); // unordered_set에서 메모리를 최소화하는 꼼수
    }

    // 요소만 지우고 메모리 공간은 유지 (재사용 시 빠름)
    void Reset()
    {
        Data.clear();
    }

    // ==========================================
    // 4. 언리얼 특유의 유틸리티 (Utilities)
    // ==========================================

    // 🔥 실무에서 진짜 많이 쓰는 기능!
    // Set에 있는 데이터를 순서 상관없이 TArray로 쫙 뽑아줍니다.
    TArray<T> Array() const
    {
        TArray<T> Result;
        for (const T& Item : Data)
        {
            Result.Add(Item);
        }
        return Result;
    }

    // ==========================================
    // 5. C++ 범위 기반 for 문 지원 (Range-based for)
    // ==========================================
    auto begin() { return Data.begin(); }
    auto end() { return Data.end(); }
    auto begin() const { return Data.begin(); }
    auto end() const { return Data.end(); }
};