#pragma once

#include "Core/CoreTypes.h"

// Forward declarations
class UObject;
class UPrimitiveComponent;
class USkeletalMesh;
class USkeleton;
class UPhysicsAsset;
class UPhysicsBodySetup;
class UPhysicalMaterial;
class AActor;

struct FHitResult;
struct FPhysicsAggregateShapeSetup;
struct FPhysicsShapeDesc;
struct FPhysicsBodyDesc;
struct FPhysicsConstraintDesc;
struct FRagdollBuildDesc;

class FPhysicsBodyInstance;
class FPhysicsConstraintInstance;
class FPhysicsScene;
class IPhysicsScene;
class IPhysicsSceneInterface;

/** Physics 전체에서 공통으로 사용하는 Shape 분류 */
enum class EPhysicsShapeType : uint8
{
    PST_Sphere,
    PST_Box,
    PST_Capsule,
    PST_Convex
};

/** Physics Body 동작 방식 */
enum class EPhysicsBodyType : uint8
{
    PBT_Static,
    PBT_Dynamic,
    PBT_Kinematic
};

/** Physics Joint / Constraint 종류 */
enum class EPhysicsJointType : uint8
{
    PJT_Fixed,
    PJT_Distance,
    PJT_Revolute,
    PJT_Prismatic,
    PJT_Spherical,
    PJT_D6
};
