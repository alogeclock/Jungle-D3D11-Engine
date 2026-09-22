#pragma once

#include "Object/FName.h"
#include "Physics/Common/PhysicalMaterialManager.h"
#include "Physics/Common/PhysicsDescTypes.h"
#include "Serialization/Archive.h"

/**
 * @file PhysicsShapeSetup.h
 * @brief PhysicsAsset에 저장되는 Collision Shape 설정 정의.
 */

/** Physical Material 참조를 경로로 직렬화한다. */
inline FString GetPhysicalMaterialAssetPath(UPhysicalMaterial* PhysicalMaterial)
{
    return PhysicalMaterial ? PhysicalMaterial->GetAssetPathFileName() : FString();
}

inline void SerializePhysicalMaterialReference(FArchive& Ar, UPhysicalMaterial*& PhysicalMaterial)
{
    FString PhysicalMaterialPath = Ar.IsSaving()
        ? GetPhysicalMaterialAssetPath(PhysicalMaterial)
        : FString();
    Ar << PhysicalMaterialPath;

    if (Ar.IsLoading())
    {
        PhysicalMaterial = PhysicalMaterialPath.empty()
            ? nullptr
            : FPhysicalMaterialManager::Get().Load(PhysicalMaterialPath);
    }
}

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

inline FArchive& operator<<(FArchive& Ar, FPhysicsSphereShapeSetup& ShapeSetup)
{
    Ar << ShapeSetup.Name;
    Ar << ShapeSetup.LocalTransform.Location;
    Ar << ShapeSetup.LocalTransform.Rotation;
    Ar << ShapeSetup.LocalTransform.Scale;
    Ar << ShapeSetup.Radius;

    SerializePhysicalMaterialReference(Ar, ShapeSetup.PhysicalMaterial);

    Ar << ShapeSetup.CollisionDesc;
    return Ar;
}

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

inline FArchive& operator<<(FArchive& Ar, FPhysicsBoxShapeSetup& ShapeSetup)
{
    Ar << ShapeSetup.Name;
    Ar << ShapeSetup.LocalTransform.Location;
    Ar << ShapeSetup.LocalTransform.Rotation;
    Ar << ShapeSetup.LocalTransform.Scale;
    Ar << ShapeSetup.HalfExtent;

    SerializePhysicalMaterialReference(Ar, ShapeSetup.PhysicalMaterial);

    Ar << ShapeSetup.CollisionDesc;
    return Ar;
}

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

inline FArchive& operator<<(FArchive& Ar, FPhysicsCapsuleShapeSetup& ShapeSetup)
{
    Ar << ShapeSetup.Name;
    Ar << ShapeSetup.LocalTransform.Location;
    Ar << ShapeSetup.LocalTransform.Rotation;
    Ar << ShapeSetup.LocalTransform.Scale;
    Ar << ShapeSetup.Radius;
    Ar << ShapeSetup.Length;

    SerializePhysicalMaterialReference(Ar, ShapeSetup.PhysicalMaterial);

    Ar << ShapeSetup.CollisionDesc;
    return Ar;
}

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

inline FArchive& operator<<(FArchive& Ar, FPhysicsConvexShapeSetup& ShapeSetup)
{
    Ar << ShapeSetup.Name;
    Ar << ShapeSetup.LocalTransform.Location;
    Ar << ShapeSetup.LocalTransform.Rotation;
    Ar << ShapeSetup.LocalTransform.Scale;
    Ar << ShapeSetup.VertexData;

    SerializePhysicalMaterialReference(Ar, ShapeSetup.PhysicalMaterial);

    Ar << ShapeSetup.CollisionDesc;
    return Ar;
}

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

inline FArchive& operator<<(FArchive& Ar, FPhysicsAggregateShapeSetup& ShapeSetup)
{
    SerializeArrayElements(Ar, ShapeSetup.SphereShapeSetups);
    SerializeArrayElements(Ar, ShapeSetup.BoxShapeSetups);
    SerializeArrayElements(Ar, ShapeSetup.CapsuleShapeSetups);
    SerializeArrayElements(Ar, ShapeSetup.ConvexShapeSetups);
    return Ar;
}
