#pragma once

#include "VehicleTypes.h"

/** Suspension 설정 */
struct FVehicleSuspensionDesc
{
    float RestLength     = 40.0f;
    float Stiffness      = 20000.0f;
    float Damping        = 3000.0f;
    float MaxCompression = 20.0f;
    float MaxDroop       = 20.0f;
};

/** Wheel 설정 */
struct FVehicleWheelDesc
{
    FName              BoneName      = FName::None;
    FVector            LocalPosition = FVector::ZeroVector;
    float              Radius        = 30.0f;
    float              Width         = 20.0f;
    EVehicleWheelRole  WheelRole     = EVehicleWheelRole::VWR_None;
    FVehicleSuspensionDesc Suspension;
};
