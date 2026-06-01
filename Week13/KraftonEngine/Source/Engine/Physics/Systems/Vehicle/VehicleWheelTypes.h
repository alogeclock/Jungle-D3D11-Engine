#pragma once

#include "VehicleTypes.h"

/** Suspension 설정 */
struct FVehicleSuspensionDesc
{
    float RestLength     = 0.4f;
    float Stiffness      = 20000.0f;
    float Damping        = 3000.0f;
    float MaxCompression = 0.2f;
    float MaxDroop       = 0.2f;
};

/** Wheel 설정 */
struct FVehicleWheelDesc
{
    FName              BoneName      = FName::None;
    FVector            LocalPosition = FVector::ZeroVector;
    float              Radius        = 0.3f;
    float              Width         = 0.2f;
    EVehicleWheelRole  WheelRole     = EVehicleWheelRole::VWR_None;
    FVehicleSuspensionDesc Suspension;
};
