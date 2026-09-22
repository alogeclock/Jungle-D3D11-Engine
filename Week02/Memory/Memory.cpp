#include "Memory.h"
#include <cstdlib> // std::malloc, std::free
#include <new>     // std::bad_alloc

// 실제 전역 변수 메모리 초기화
uint32 TotalAllocationBytes = 0;
uint32 TotalAllocationCount = 0;

// ---------------------------------------------------------
// 엔진 전용 메모리 헤더 (정렬 보호)
// ---------------------------------------------------------
// x64 환경에서 포인터 연산 정렬(16바이트 얼라인먼트)이 깨지지 않도록
// Size 이외에 Padding을 넣어 16바이트 크기를 맞춥니다. (SIMD/DirectX 크래시 방지)
struct FMemoryHeader
{
    size_t Size;
    size_t Padding;
};

// ---------------------------------------------------------
// 메모리 할당 로직
// ---------------------------------------------------------
void *operator new(size_t Size)
{
    if (Size == 0)
    {
        Size = 1;
    }

    // 1. 사용자가 요청한 크기에 '헤더 크기'를 더해서 진짜 메모리를 할당합니다.
    size_t TotalSize = Size + sizeof(FMemoryHeader);
    void  *RawMemory = std::malloc(TotalSize);

    if (!RawMemory)
    {
        throw std::bad_alloc();
    }

    // 2. 할당받은 메모리의 맨 앞부분을 헤더로 형변환하고, 크기를 기록합니다.
    FMemoryHeader *Header = static_cast<FMemoryHeader *>(RawMemory);
    Header->Size = Size;

    // 3. 엔진 통계 업데이트 (헤더 크기를 뺀 순수 데이터 크기만 기록)
    TotalAllocationBytes += static_cast<uint32>(Size);
    TotalAllocationCount++;

    // 4. ⭐️ 사용자에게는 헤더 공간(16바이트)을 건너뛴, 진짜 데이터 영역의 시작 주소를 줍니다!
    return static_cast<void *>(Header + 1);
}

void *operator new[](size_t Size)
{
    // 배열 할당도 내부적으로 단일 할당 로직을 탑니다.
    return ::operator new(Size);
}

// ---------------------------------------------------------
// 메모리 해제 로직 (크기 정보가 없는 delete 완벽 대응)
// ---------------------------------------------------------
void operator delete(void *Memory) noexcept
{
    if (!Memory)
        return;

    // 1. ⭐️ 사용자가 넘겨준 주소에서 뒤로 한 칸(헤더 크기만큼) 물러나서 우리가 숨겨둔 헤더를 찾습니다.
    FMemoryHeader *Header = static_cast<FMemoryHeader *>(Memory) - 1;

    // 2. 숨겨둔 크기 정보를 빼옵니다.
    size_t Size = Header->Size;

    // 3. 엔진 통계 차감 (이제 완벽하게 크기를 깎을 수 있습니다!)
    if (TotalAllocationBytes >= Size)
    {
        TotalAllocationBytes -= static_cast<uint32>(Size);
    }
    if (TotalAllocationCount > 0)
    {
        TotalAllocationCount--;
    }

    // 4. malloc이 반환했던 '진짜 시작 주소(Header)'를 free 합니다.
    std::free(Header);
}

void operator delete[](void *Memory) noexcept { ::operator delete(Memory); }

// ---------------------------------------------------------
// 크기 정보가 있는 delete 호출도 위쪽의 헤더 기반 로직으로 통일시킵니다.
// ---------------------------------------------------------
void operator delete(void *Memory, size_t /*Size*/) noexcept
{
    // C++14 이상에서 호출되는 sized delete도, 우리가 만든 완벽한 unsized delete로 넘깁니다.
    ::operator delete(Memory);
}

void operator delete[](void *Memory, size_t /*Size*/) noexcept { ::operator delete(Memory); }