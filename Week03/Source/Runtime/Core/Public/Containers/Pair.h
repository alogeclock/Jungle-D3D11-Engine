#pragma once
#include <utility> // std::pair, std::make_pair 를 위해 필요

template <typename KeyInitType, typename ValueInitType>
struct TPair
{
public:
    // 🔥 언리얼 엔진 TPair의 핵심! first, second가 아니라 Key, Value 입니다.
    KeyInitType Key;
    ValueInitType Value;

    // ==========================================
    // 1. 생성자 (Constructors)
    // ==========================================

    // 기본 생성자
    TPair() = default;

    // Key, Value를 직접 넣어서 생성
    TPair(const KeyInitType& InKey, const ValueInitType& InValue)
        : Key(InKey), Value(InValue)
    {
    }

    // ==========================================
    // 2. STL 호환성 (std::pair와의 브릿지)
    // ==========================================

    // std::pair를 TPair로 자동 변환해서 받아줌
    TPair(const std::pair<KeyInitType, ValueInitType>& StdPair)
        : Key(StdPair.first), Value(StdPair.second)
    {
    }

    // TPair를 std::pair로 자동 변환해서 내보냄
    // (이전에 만든 TMap의 내부 unordered_map과 연동할 때 아주 유용합니다)
    operator std::pair<KeyInitType, ValueInitType>() const
    {
        return std::make_pair(Key, Value);
    }
};

// ==========================================
// 3. 언리얼식 헬퍼 함수 (std::make_pair 래핑)
// ==========================================
template <typename KeyType, typename ValueType>
inline TPair<KeyType, ValueType> MakeTuple(const KeyType& InKey, const ValueType& InValue)
{
    return TPair<KeyType, ValueType>(InKey, InValue);
}