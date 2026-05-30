#pragma once

#include "VehicleInputTypes.h"

/** Vehicle Debug Draw 표시 옵션 */
struct FVehicleDebugDrawOptions
{
    bool bDrawWheel      = true;
    bool bDrawSuspension = true;
    bool bDrawContact    = true;
    bool bDrawDriveForce = false;
};

/** Vehicle Runtime 통계 정보 */
struct FVehicleRuntimeStats
{
    int32 WheelCount     = 0;
    float CurrentSpeed   = 0.0f;
    float EngineTorque   = 0.0f;
    float BrakeTorque    = 0.0f;
    float SteeringAngle  = 0.0f;
};
