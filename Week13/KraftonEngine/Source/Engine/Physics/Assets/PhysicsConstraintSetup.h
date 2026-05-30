#pragma once

#include "Object/FName.h"
#include "Physics/Common/PhysicsDescTypes.h"

/**
 * @file PhysicsConstraintSetup.h
 * @brief PhysicsAsset에 저장되는 Constraint 설정 정의.
 */

/** Parent Body와 Child Body 사이의 Constraint 설정 */
struct FPhysicsConstraintSetup
{
    FName ConstraintName = FName::None;
    FName ParentBoneName = FName::None;
    FName ChildBoneName  = FName::None;

    EPhysicsJointType JointType = EPhysicsJointType::PJT_D6;

    FTransform ParentLocalFrame;
    FTransform ChildLocalFrame;

    float LinearLimit       =   0.0f;
    float AngularLimit      =   0.0f;
    float TwistLimitMin     = -45.0f;
    float TwistLimitMax     =  45.0f;
    float SwingLimitY       =  30.0f;
    float SwingLimitZ       =  30.0f;
    bool  bDisableCollision = false;
    float Stiffness         =   0.0f;
    float Damping           =   0.0f;

    FPhysicsConstraintDesc BuildConstraintDesc() const
    {
        FPhysicsConstraintDesc Desc;
        Desc.JointType         = JointType;
        Desc.ParentLocalFrame  = ParentLocalFrame;
        Desc.ChildLocalFrame   = ChildLocalFrame;
        Desc.LinearLimit       = LinearLimit;
        Desc.AngularLimit      = AngularLimit;
        Desc.TwistLimitMin     = TwistLimitMin;
        Desc.TwistLimitMax     = TwistLimitMax;
        Desc.SwingLimitY       = SwingLimitY;
        Desc.SwingLimitZ       = SwingLimitZ;
        Desc.bDisableCollision = bDisableCollision;
        Desc.Stiffness         = Stiffness;
        Desc.Damping           = Damping;
        return Desc;
    }
};
