#pragma once

#include "Core/CoreTypes.h"
#include "Math/Matrix.h"

#include <foundation/PxVec4.h>

class AActor;
class UPrimitiveComponent;

struct FClothCollisionData
{
    TArray<physx::PxVec4> Spheres;   // (x, y, z, radius) in cloth simulation space
    TArray<uint32> Capsules;         // sphere index pair
    TArray<physx::PxVec4> Planes;    // (normal.xyz, d) in cloth simulation space
    TArray<uint32> ConvexMasks;      // bit mask over Planes

    void Reset()
    {
        Spheres.clear();
        Capsules.clear();
        Planes.clear();
        ConvexMasks.clear();
    }
};

struct FClothCollisionGatherParams
{
    FMatrix WorldToCloth = FMatrix::Identity;
    UPrimitiveComponent* IgnoreComponent = nullptr;
    AActor* IgnoreActor = nullptr;
    bool bIncludeStatic = true;
    bool bIncludeDynamic = true;
    uint32 MaxSpheres = 32;
    uint32 MaxPlanes = 32;
};
