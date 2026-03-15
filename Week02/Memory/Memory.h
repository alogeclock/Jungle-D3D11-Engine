#pragma once
#include "CoreTypes.h" // uint32 등의 타입 정의가 포함된 헤더
#include <new>         // std::bad_alloc 예외 처리를 위해 필요

// ---------------------------------------------------------
// 전역 메모리 통계 변수 (다른 파일에서 extern으로 접근 가능)
// ---------------------------------------------------------
extern uint32 TotalAllocationBytes;
extern uint32 TotalAllocationCount;

// ---------------------------------------------------------
// 전역 메모리 할당/해제 연산자 오버로딩
// ---------------------------------------------------------

// 단일 객체 할당 (예: new UCubeComp)
void* operator new(size_t Size);

// 배열 할당 (예: new int[10])
void* operator new[](size_t Size);

// 단일 객체 해제 (C++14 크기 지정 해제 적용)
void operator delete(void* Memory, size_t Size) noexcept;

// 배열 해제 (C++14 크기 지정 해제 적용)
void operator delete[](void* Memory, size_t Size) noexcept;

// (참고) 크기를 모르는 일반 delete 호환을 위한 오버로딩
void operator delete(void* Memory) noexcept;
void operator delete[](void* Memory) noexcept;