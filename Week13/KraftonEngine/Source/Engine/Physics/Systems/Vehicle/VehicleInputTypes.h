#pragma once

#include "VehicleWheelTypes.h"

class UStaticMesh;
class USkeletalMesh;

/** 차량 구동 설정 */
struct FVehicleDriveDesc
{
    EVehicleDriveType DriveType       = EVehicleDriveType::VDT_RearWheel;
    float             MaxEngineTorque = 1500.0f;
    float             MaxEngineOmega  = 500.0f;
    float             MaxBrakeTorque  = 3000.0f;
    float             HandbrakeTorque = 4000.0f;
    float             ClutchStrength  = 10.0f;
    float             MaxSteerAngle   = 35.0f;
    float             ChassisMass     = 1200.0f;
    FVector           CenterOfMassOffset = FVector(0.0f, 0.0f, -0.25f);
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
