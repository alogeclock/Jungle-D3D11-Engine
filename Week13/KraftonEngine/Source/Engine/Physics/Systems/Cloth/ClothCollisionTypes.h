#pragma once

#include "Core/CoreTypes.h"

#include <foundation/PxVec3.h>
#include <foundation/PxVec4.h>

struct FClothCollisionData
{
    TArray<physx::PxVec4> Spheres;
    TArray<uint32> Capsules;
    TArray<physx::PxVec4> Planes;
    TArray<uint32> ConvexMasks;
    TArray<physx::PxVec3> Triangles;
};
