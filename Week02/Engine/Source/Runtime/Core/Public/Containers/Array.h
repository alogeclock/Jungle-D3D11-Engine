#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

// 언리얼 기본 자료형 매핑 (CoreTypes.h에 있다면 생략 가능)
typedef int32_t int32;

// ==========================================
// 1. 동적 배열 (TArray) -> std::vector 래핑
// ==========================================
template <typename T>
class TArray
{
private:
    std::vector<T> Data;

public:
    // 언리얼 특유의 데이터 추가 함수
    void Add(const T& Item) { Data.push_back(Item); }
    void Emplace(T&& Item) { Data.emplace_back(std::forward<T>(Item)); }

    // 크기 반환 (STL의 size() 대신 언리얼은 Num()을 씁니다)
    int32 Num() const { return static_cast<int32>(Data.size()); }

    // 배열 비우기
    void Empty() { Data.clear(); }

    // 인덱스 유효성 검사
    bool IsValidIndex(int32 Index) const { return Index >= 0 && Index < Num(); }

    // 연산자 오버로딩 (인덱스 접근)
    T& operator[](int32 Index) { return Data[Index]; }
    const T& operator[](int32 Index) const { return Data[Index]; }

    // C++11 Range-based for loop 지원을 위한 이터레이터
    auto begin() { return Data.begin(); }
    auto end() { return Data.end(); }
    auto begin() const { return Data.begin(); }
    auto end() const { return Data.end(); }
};


// ==========================================
// 2. 문자열 (FString) -> std::string 래핑
// ==========================================
class FString
{
private:
    std::string Data;

public:
    FString() = default;
    FString(const char* InStr) : Data(InStr) {}
    FString(const std::string& InStr) : Data(InStr) {}

    // 문자열 길이 (length 대신 Len)
    int32 Len() const { return static_cast<int32>(Data.length()); }

    // 문자열 비우기
    void Empty() { Data.clear(); }

    // 언리얼의 C-String 접근자 (*FString 하면 C 문자열이 나옴)
    const char* operator*() const { return Data.c_str(); }

    // 문자열 더하기
    FString& operator+=(const FString& Other)
    {
        Data += Other.Data;
        return *this;
    }
};


// ==========================================
// 3. 해시 맵 (TMap) -> std::unordered_map 래핑
// ==========================================
template <typename KeyType, typename ValueType>
class TMap
{
private:
    std::unordered_map<KeyType, ValueType> Data;

public:
    // 데이터 추가 (STL의 insert나 [] 대신 Add 사용)
    void Add(const KeyType& InKey, const ValueType& InValue)
    {
        Data[InKey] = InValue;
    }

    // 키가 존재하는지 확인
    bool Contains(const KeyType& InKey) const
    {
        return Data.find(InKey) != Data.end();
    }

    // 요소 개수
    int32 Num() const { return static_cast<int32>(Data.size()); }

    // 비우기
    void Empty() { Data.clear(); }

    // 값 접근 (언리얼식 인덱싱)
    ValueType& operator[](const KeyType& InKey) { return Data[InKey]; }
};