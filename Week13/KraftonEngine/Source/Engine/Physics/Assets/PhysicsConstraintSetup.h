#pragma once

#include "Object/FName.h"
#include "Physics/Common/PhysicsDescTypes.h"
#include "Serialization/Archive.h"

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
    bool  bDisableCollision =  false;
    float Stiffness         =   0.0f;
    float Damping           =   0.0f;

    bool bParentDominates        = false;
    bool bEnableMassConditioning = true;
    bool bUseLinearJointSolver   = true;
    bool bScaleLinearLimits      = true;

    EPhysicsConstraintMotionMode XMotion      = EPhysicsConstraintMotionMode::Locked;
    EPhysicsConstraintMotionMode YMotion      = EPhysicsConstraintMotionMode::Locked;
    EPhysicsConstraintMotionMode ZMotion      = EPhysicsConstraintMotionMode::Locked;
    EPhysicsConstraintMotionMode Swing1Motion = EPhysicsConstraintMotionMode::Limited;
    EPhysicsConstraintMotionMode Swing2Motion = EPhysicsConstraintMotionMode::Limited;
    EPhysicsConstraintMotionMode TwistMotion  = EPhysicsConstraintMotionMode::Limited;

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
        Desc.XMotion           = XMotion;
        Desc.YMotion           = YMotion;
        Desc.ZMotion           = ZMotion;
        Desc.Swing1Motion      = Swing1Motion;
        Desc.Swing2Motion      = Swing2Motion;
        Desc.TwistMotion       = TwistMotion;
        return Desc;
    }
};

inline FArchive& operator<<(FArchive& Ar, FPhysicsConstraintSetup& ConstraintSetup)
{
    constexpr uint32 VersionTag = 0x50435354u;
    constexpr uint32 Version    = 2u;

    uint32 SerializedTag     = VersionTag;
    uint32 SerializedVersion = Version;
    if (Ar.IsSaving())
    {
        Ar << SerializedTag;
        Ar << SerializedVersion;
    }
    else
    {
        Ar << SerializedTag;
        if (SerializedTag == VersionTag)
        {
            Ar << SerializedVersion;
        }
        else
        {
            SerializedVersion = 0u;
        }
    }

    Ar << ConstraintSetup.ConstraintName;
    Ar << ConstraintSetup.ParentBoneName;
    Ar << ConstraintSetup.ChildBoneName;

    uint8 JointType = static_cast<uint8>(ConstraintSetup.JointType);
    Ar << JointType;

    Ar << ConstraintSetup.ParentLocalFrame.Location;
    Ar << ConstraintSetup.ParentLocalFrame.Rotation;
    Ar << ConstraintSetup.ParentLocalFrame.Scale;
    Ar << ConstraintSetup.ChildLocalFrame.Location;
    Ar << ConstraintSetup.ChildLocalFrame.Rotation;
    Ar << ConstraintSetup.ChildLocalFrame.Scale;

    Ar << ConstraintSetup.LinearLimit;
    Ar << ConstraintSetup.AngularLimit;
    Ar << ConstraintSetup.TwistLimitMin;
    Ar << ConstraintSetup.TwistLimitMax;
    Ar << ConstraintSetup.SwingLimitY;
    Ar << ConstraintSetup.SwingLimitZ;
    Ar << ConstraintSetup.bDisableCollision;
    Ar << ConstraintSetup.Stiffness;
    Ar << ConstraintSetup.Damping;

    if (SerializedVersion >= 2u || Ar.IsSaving())
    {
        uint8 XMotion      = static_cast<uint8>(ConstraintSetup.XMotion);
        uint8 YMotion      = static_cast<uint8>(ConstraintSetup.YMotion);
        uint8 ZMotion      = static_cast<uint8>(ConstraintSetup.ZMotion);
        uint8 Swing1Motion = static_cast<uint8>(ConstraintSetup.Swing1Motion);
        uint8 Swing2Motion = static_cast<uint8>(ConstraintSetup.Swing2Motion);
        uint8 TwistMotion  = static_cast<uint8>(ConstraintSetup.TwistMotion);
        Ar << ConstraintSetup.bParentDominates;
        Ar << ConstraintSetup.bEnableMassConditioning;
        Ar << ConstraintSetup.bUseLinearJointSolver;
        Ar << ConstraintSetup.bScaleLinearLimits;
        Ar << XMotion;
        Ar << YMotion;
        Ar << ZMotion;
        Ar << Swing1Motion;
        Ar << Swing2Motion;
        Ar << TwistMotion;

        if (Ar.IsLoading())
        {
            ConstraintSetup.XMotion      = static_cast<EPhysicsConstraintMotionMode>(XMotion);
            ConstraintSetup.YMotion      = static_cast<EPhysicsConstraintMotionMode>(YMotion);
            ConstraintSetup.ZMotion      = static_cast<EPhysicsConstraintMotionMode>(ZMotion);
            ConstraintSetup.Swing1Motion = static_cast<EPhysicsConstraintMotionMode>(Swing1Motion);
            ConstraintSetup.Swing2Motion = static_cast<EPhysicsConstraintMotionMode>(Swing2Motion);
            ConstraintSetup.TwistMotion  = static_cast<EPhysicsConstraintMotionMode>(TwistMotion);
        }
    }
    else if (Ar.IsLoading())
    {
        ConstraintSetup.bParentDominates        = false;
        ConstraintSetup.bEnableMassConditioning = true;
        ConstraintSetup.bUseLinearJointSolver   = true;
        ConstraintSetup.bScaleLinearLimits      = true;
        ConstraintSetup.XMotion                 = EPhysicsConstraintMotionMode::Locked;
        ConstraintSetup.YMotion                 = EPhysicsConstraintMotionMode::Locked;
        ConstraintSetup.ZMotion                 = EPhysicsConstraintMotionMode::Locked;
        ConstraintSetup.Swing1Motion            = EPhysicsConstraintMotionMode::Limited;
        ConstraintSetup.Swing2Motion            = EPhysicsConstraintMotionMode::Limited;
        ConstraintSetup.TwistMotion             = EPhysicsConstraintMotionMode::Limited;
    }

    if (Ar.IsLoading())
    {
        ConstraintSetup.JointType = static_cast<EPhysicsJointType>(JointType);
    }

    return Ar;
}
