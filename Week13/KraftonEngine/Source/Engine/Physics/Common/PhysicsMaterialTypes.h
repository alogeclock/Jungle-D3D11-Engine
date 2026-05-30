#pragma once

#include "Object/Object.h"
#include "PhysicsTypes.h"

/**
 * @file PhysicsMaterialTypes.h
 * @brief 물리 재질 설정과 물리 재질 Asset 정의.
 */

/** 물리 재질 설정 Desc */
struct FPhysicsMaterialDesc
{
    float StaticFriction  = 0.5f;
    float DynamicFriction = 0.5f;
    float Restitution     = 0.6f;
    float Density         = 1.0f;
};

/** 물리 재질 Asset */
class UPhysicalMaterial : public UObject
{
public:
    const FPhysicsMaterialDesc& GetMaterialDesc() const { return MaterialDesc; }
    void SetMaterialDesc(const FPhysicsMaterialDesc& InDesc) { MaterialDesc = InDesc; }

    float GetStaticFriction() const { return MaterialDesc.StaticFriction; }
    float GetDynamicFriction() const { return MaterialDesc.DynamicFriction; }
    float GetRestitution() const { return MaterialDesc.Restitution; }
    float GetDensity() const { return MaterialDesc.Density; }

private:
    FPhysicsMaterialDesc MaterialDesc;
};
