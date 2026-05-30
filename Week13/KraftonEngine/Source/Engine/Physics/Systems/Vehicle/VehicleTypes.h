#pragma once

#include "Physics/Common/PhysicsDescTypes.h"

/** 차량 구동 방식 */
enum class EVehicleDriveType : uint8
{
    VDT_FrontWheel, // 전륜 구동
    VDT_RearWheel,  // 후륜 구동
    VDT_FourWheel   // 사륜 구동
};

/** Wheel 역할 */
enum class EVehicleWheelRole : uint8
{
    VWR_None,       // 특수 역할 없음
    VWR_Steering,   // 조향 Wheel
    VWR_Drive,      // 구동 Wheel
    VWR_SteerDrive  // 조향 + 구동 Wheel
};

/** 차량 입력 상태 */
struct FVehicleInputState
{
    float Throttle  = 0.0f;
    float Brake     = 0.0f;
    float Steering  = 0.0f;
    bool  bHandbrake = false;
};
