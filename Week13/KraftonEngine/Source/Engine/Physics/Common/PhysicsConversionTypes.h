#pragma once

#include "Object/FName.h"
#include "PhysicsDescTypes.h"

/**
 * @file PhysicsConversionTypes.h
 * @brief 엔진 타입과 Physics Runtime 타입 사이의 변환 기준 정의.
 */

/** Physics 단위 변환 기준 */
struct FPhysicsUnitScale
{
    float LengthScale = 1.0f;
    float MassScale   = 1.0f;
    float TimeScale   = 1.0f;
};

/** Bone Transform 기반 Body 생성 정보 */
struct FPhysicsTransformBuildInfo
{
    FName      BoneName           = FName::None;
    int32      BoneIndex          = INDEX_NONE;
    FTransform BoneWorldTransform;
    FTransform BoneLocalTransform;
};
