#pragma once

#include "Object/FName.h"
#include "Physics/Common/PhysicsDescTypes.h"

/**
 * @file PhysicsShapeSetup.h
 * @brief PhysicsAsset에 저장되는 Collision Shape 설정 정의.
 */

/** Sphere Collision Shape 설정 */
struct FPhysicsSphereShapeSetup
{
    FName                 Name             = FName::None;
    FTransform            LocalTransform;
    float                 Radius           = 15.0f;
    UPhysicalMaterial*    PhysicalMaterial = nullptr;
    FPhysicsCollisionDesc CollisionDesc;

    FPhysicsShapeDesc BuildShapeDesc() const
    {
        FPhysicsShapeDesc Desc;
        Desc.ShapeType        = EPhysicsShapeType::PST_Sphere;
        Desc.LocalTransform   = LocalTransform;
        Desc.Size             = FVector(Radius, Radius, Radius);
        Desc.PhysicalMaterial = PhysicalMaterial;
        Desc.CollisionDesc    = CollisionDesc;
        return Desc;
    }
};

/** Box Collision Shape 설정 */
struct FPhysicsBoxShapeSetup
{
    FName                 Name             = FName::None;
    FTransform            LocalTransform;
    FVector               HalfExtent       = FVector(10.0f, 10.0f, 10.0f);
    UPhysicalMaterial*    PhysicalMaterial = nullptr;
    FPhysicsCollisionDesc CollisionDesc;

    FPhysicsShapeDesc BuildShapeDesc() const
    {
        FPhysicsShapeDesc Desc;
        Desc.ShapeType        = EPhysicsShapeType::PST_Box;
        Desc.LocalTransform   = LocalTransform;
        Desc.Size             = HalfExtent;
        Desc.PhysicalMaterial = PhysicalMaterial;
        Desc.CollisionDesc    = CollisionDesc;
        return Desc;
    }
};

/** Capsule Collision Shape 설정 */
struct FPhysicsCapsuleShapeSetup
{
    FName                 Name             = FName::None;
    FTransform            LocalTransform;
    float                 Radius           = 10.0f;
    float                 Length           = 30.0f;
    UPhysicalMaterial*    PhysicalMaterial = nullptr;
    FPhysicsCollisionDesc CollisionDesc;

    FPhysicsShapeDesc BuildShapeDesc() const
    {
        FPhysicsShapeDesc Desc;
        Desc.ShapeType        = EPhysicsShapeType::PST_Capsule;
        Desc.LocalTransform   = LocalTransform;
        Desc.Size             = FVector(Radius, Length, 0.0f);
        Desc.PhysicalMaterial = PhysicalMaterial;
        Desc.CollisionDesc    = CollisionDesc;
        return Desc;
    }
};

/** Convex Collision Shape 설정 */
struct FPhysicsConvexShapeSetup
{
    FName                 Name             = FName::None;
    FTransform            LocalTransform;
    TArray<FVector>       VertexData;
    UPhysicalMaterial*    PhysicalMaterial = nullptr;
    FPhysicsCollisionDesc CollisionDesc;

    FPhysicsShapeDesc BuildShapeDesc() const
    {
        FPhysicsShapeDesc Desc;
        Desc.ShapeType        = EPhysicsShapeType::PST_Convex;
        Desc.LocalTransform   = LocalTransform;
        Desc.VertexData       = VertexData;
        Desc.PhysicalMaterial = PhysicalMaterial;
        Desc.CollisionDesc    = CollisionDesc;
        return Desc;
    }
};

/** 하나의 Body에 포함되는 Collision Shape 묶음 */
struct FPhysicsAggregateShapeSetup
{
    TArray<FPhysicsSphereShapeSetup>  SphereShapeSetups;
    TArray<FPhysicsBoxShapeSetup>     BoxShapeSetups;
    TArray<FPhysicsCapsuleShapeSetup> CapsuleShapeSetups;
    TArray<FPhysicsConvexShapeSetup>  ConvexShapeSetups;

    TArray<FPhysicsShapeDesc> BuildShapeDescs() const
    {
        TArray<FPhysicsShapeDesc> ShapeDescs;
        ShapeDescs.reserve(
            SphereShapeSetups.size() +
            BoxShapeSetups.size() +
            CapsuleShapeSetups.size() +
            ConvexShapeSetups.size());

        for (const FPhysicsSphereShapeSetup& ShapeSetup : SphereShapeSetups)
            ShapeDescs.push_back(ShapeSetup.BuildShapeDesc());
        for (const FPhysicsBoxShapeSetup& ShapeSetup : BoxShapeSetups)
            ShapeDescs.push_back(ShapeSetup.BuildShapeDesc());
        for (const FPhysicsCapsuleShapeSetup& ShapeSetup : CapsuleShapeSetups)
            ShapeDescs.push_back(ShapeSetup.BuildShapeDesc());
        for (const FPhysicsConvexShapeSetup& ShapeSetup : ConvexShapeSetups)
            ShapeDescs.push_back(ShapeSetup.BuildShapeDesc());

        return ShapeDescs;
    }

    bool IsEmpty() const
    {
        return SphereShapeSetups.empty() &&
               BoxShapeSetups.empty() &&
               CapsuleShapeSetups.empty() &&
               ConvexShapeSetups.empty();
    }
};
