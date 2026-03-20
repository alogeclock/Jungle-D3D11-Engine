#pragma once
#include <list>
#include <algorithm>
#include <stdexcept>

// CoreTypes.h에 정의되어 있다고 가정하는 기본 타입
typedef int32_t int32;

template <typename T>
class TDoubleLinkedList
{
private:
    // std::list는 이중 연결 리스트이므로 언리얼의 TDoubleLinkedList와 완벽히 대응됩니다.
    std::list<T> Data;

public:
    // ==========================================
    // 1. 데이터 추가 (Add)
    // ==========================================

    // 리스트의 맨 앞에 추가 (push_front)
    void AddHead(const T& Element)
    {
        Data.push_front(Element);
    }

    // 리스트의 맨 뒤에 추가 (push_back)
    void AddTail(const T& Element)
    {
        Data.push_back(Element);
    }

    // ==========================================
    // 2. 정보 및 상태 확인 (Info & Status)
    // ==========================================

    // 요소의 개수 반환 (size)
    // 주의: std::list의 size()는 C++11부터 O(1)이지만, 이전엔 O(N)이었습니다.
    int32 Num() const { return static_cast<int32>(Data.size()); }

    // 비어있는가?
    bool IsEmpty() const { return Data.empty(); }

    // 특정 요소를 포함하고 있는가?
    bool Contains(const T& Element) const
    {
        return std::find(Data.begin(), Data.end(), Element) != Data.end();
    }

    // ==========================================
    // 3. 데이터 삭제 (Remove)
    // ==========================================

    // 특정 값을 가진 노드를 모두 삭제 (remove)
    void Remove(const T& Element)
    {
        Data.remove(Element);
    }

    // 리스트를 완전히 비움 (clear)
    void Empty()
    {
        Data.clear();
    }

    // ==========================================
    // 4. 데이터 접근 (Accessors)
    // ==========================================

    // 맨 앞 요소 가져오기
    T& GetHead()
    {
        if (IsEmpty()) throw std::out_of_range("List is empty!");
        return Data.front();
    }
    const T& GetHead() const
    {
        if (IsEmpty()) throw std::out_of_range("List is empty!");
        return Data.front();
    }

    // 맨 뒤 요소 가져오기
    T& GetTail()
    {
        if (IsEmpty()) throw std::out_of_range("List is empty!");
        return Data.back();
    }
    const T& GetTail() const
    {
        if (IsEmpty()) throw std::out_of_range("List is empty!");
        return Data.back();
    }

    // ==========================================
    // 5. C++ 범위 기반 for 문 지원 (Range-based for)
    // ==========================================
    auto begin() { return Data.begin(); }
    auto end() { return Data.end(); }
    auto begin() const { return Data.begin(); }
    auto end() const { return Data.end(); }
};