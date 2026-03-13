#pragma once

// 원래는 PlatformMath.h 파일에서 #include COMPILED_PLATFORM_HEADER(PlatformMath.h)를 사용하여
// 현재 사용하는 플랫폼이 어느 플랫폼인지 확인하여 적합한 함수를 불러 오지만
// COMPILED_PLATFORM_HEADER를 사용하려면 CoreTypes.h가 필요하고 거기까지 구현은 무리이기 때문에
// 바로 GenericPlatformMath를 참조하겠음
#include "PlatformMath.h"

struct FMath : public FPlatformMath
{

};