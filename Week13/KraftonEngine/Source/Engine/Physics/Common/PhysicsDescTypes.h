#pragma once

#include "PhysicsCollisionTypes.h"
#include "PhysicsMaterialTypes.h"
#include "Math/Transform.h"
#include "Math/Vector.h"

/**
 * @file PhysicsDescTypes.h
 * @brief Runtime 물리 객체 생성을 위한 공용 Desc 정의.
 */

/** Runtime Shape 생성 정보 */
struct FPhysicsShapeDesc
{
    EPhysicsShapeType      ShapeType        = EPhysicsShapeType::PST_Capsule;
    FTransform             LocalTransform;
    FVector                Size             = FVector::OneVector;
    TArray<FVector>        VertexData;
    UPhysicalMaterial*     PhysicalMaterial = nullptr;
    FPhysicsCollisionDesc  CollisionDesc;
};

/** Runtime Body 생성 정보 */
struct FPhysicsBodyDesc
{
    EPhysicsBodyType          BodyType       = EPhysicsBodyType::PBT_Dynamic;
    float                     Mass           = 1.0f;
    float                     LinearDamping  = 0.0f;
    float                     AngularDamping = 0.05f;
    bool                      bEnableSelfCollision = false;
    FPhysicsCollisionDesc     CollisionDesc;
    TArray<FPhysicsShapeDesc> Shapes;
};

/** Constraint 축별 Motion 모드 */
enum class EPhysicsConstraintMotionMode : uint8
{
    Free = 0,
    Limited,
    Locked,
};

/** Runtime Constraint 생성 정보 */
struct FPhysicsConstraintDesc
{
    EPhysicsJointType JointType          = EPhysicsJointType::PJT_D6;
    FTransform        ParentLocalFrame;
    FTransform        ChildLocalFrame;
    float             LinearLimit        =   0.0f;
    float             AngularLimit       =   0.0f;
    float             TwistLimitMin      = -45.0f;
    float             TwistLimitMax      =  45.0f;
    float             SwingLimitY        =  30.0f;
    float             SwingLimitZ        =  30.0f;
    bool              bDisableCollision  = false;
    float             Stiffness          =   0.0f;
    float             Damping            =   0.0f;
    EPhysicsConstraintMotionMode XMotion      = EPhysicsConstraintMotionMode::Locked;
    EPhysicsConstraintMotionMode YMotion      = EPhysicsConstraintMotionMode::Locked;
    EPhysicsConstraintMotionMode ZMotion      = EPhysicsConstraintMotionMode::Locked;
    EPhysicsConstraintMotionMode Swing1Motion = EPhysicsConstraintMotionMode::Limited;
    EPhysicsConstraintMotionMode Swing2Motion = EPhysicsConstraintMotionMode::Limited;
    EPhysicsConstraintMotionMode TwistMotion  = EPhysicsConstraintMotionMode::Limited;
};
