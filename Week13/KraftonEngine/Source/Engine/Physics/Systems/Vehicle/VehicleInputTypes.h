#pragma once

#include "VehicleWheelTypes.h"

class UStaticMesh;
class USkeletalMesh;

/** 차량 구동 설정 */
struct FVehicleDriveDesc
{
    EVehicleDriveType DriveType       = EVehicleDriveType::VDT_RearWheel;
    float             MaxEngineTorque = 5000.0f;
    float             MaxBrakeTorque  = 8000.0f;
    float             MaxSteerAngle   = 35.0f;
    float             ChassisMass     = 1200.0f;
};

/** Vehicle Runtime 생성 정보 */
struct FVehicleBuildDesc
{
    USkeletalMesh*         SkeletalMesh   = nullptr;
    UStaticMesh*           ChassisMesh    = nullptr;
    FPhysicsBodyDesc       ChassisBodyDesc;
    TArray<FVehicleWheelDesc> WheelDescs;
    FVehicleDriveDesc      DriveDesc;

    bool IsValid() const
    {
        return WheelDescs.size() == 4 && DriveDesc.ChassisMass > 0.0f;
    }
};
