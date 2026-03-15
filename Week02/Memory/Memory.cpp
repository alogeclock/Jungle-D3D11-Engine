#include "Memory.h"
#include <cstdlib> // std::malloc, std::free

// 실제 전역 변수 메모리 초기화
uint32 TotalAllocationBytes = 0;
uint32 TotalAllocationCount = 0;

// ---------------------------------------------------------
// 메모리 할당 로직
// ---------------------------------------------------------
void* operator new(size_t Size)
{
    // C++ 표준에 따라 0바이트 할당 요청 시 최소 1바이트를 할당하여 고유 주소를 보장합니다.
    if (Size == 0) {
        Size = 1;
    }

    // 실제 힙 메모리 할당
    void* Memory = std::malloc(Size);

    // 할당 실패 시 예외 발생
    if (!Memory) {
        throw std::bad_alloc();
    }

    // 엔진 통계 업데이트
    TotalAllocationBytes += static_cast<uint32>(Size);
    TotalAllocationCount++;

    return Memory;
}

void* operator new[](size_t Size)
{
    // 배열 할당도 내부적으로 단일 할당 로직을 타게 하여 통계를 공유합니다.
    return ::operator new(Size);
}

// ---------------------------------------------------------
// 메모리 해제 로직
// ---------------------------------------------------------
void operator delete(void* Memory, size_t Size) noexcept
{
    if (!Memory) return;

    // 엔진 통계 차감
    if (TotalAllocationBytes >= Size) {
        TotalAllocationBytes -= static_cast<uint32>(Size);
    }
    if (TotalAllocationCount > 0) {
        TotalAllocationCount--;
    }

    // 실제 힙 메모리 해제
    std::free(Memory);
}

void operator delete[](void* Memory, size_t Size) noexcept
{
    ::operator delete(Memory, Size);
}

// ---------------------------------------------------------
// 크기 정보가 없는 delete 호출 대비 (통계 정확도는 떨어질 수 있음)
// ---------------------------------------------------------
void operator delete(void* Memory) noexcept
{
    if (!Memory) return;

    if (TotalAllocationCount > 0) {
        TotalAllocationCount--;
    }
    // 크기를 모르기 때문에 TotalAllocationBytes는 차감하지 못합니다.
    // (완벽한 추적을 원한다면 커스텀 할당자에서 크기 정보를 메모리 헤더에 기록해야 합니다)

    std::free(Memory);
}

void operator delete[](void* Memory) noexcept
{
    ::operator delete(Memory);
}