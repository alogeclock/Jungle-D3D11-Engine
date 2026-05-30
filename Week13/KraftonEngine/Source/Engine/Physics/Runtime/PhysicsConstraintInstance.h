/**
 * @file PhysicsConstraintInstance.h
 * @brief 두 Body 사이에 생성되는 Runtime Physics Constraint 상태를 정의한다.
 */
#pragma once

#include "PhysicsBodyInstance.h"

/** Runtime에서 실제로 생성된 Physics Constraint 상태 */
class FPhysicsConstraintInstance
{
public:
    FPhysicsConstraintInstance()  = default;
    ~FPhysicsConstraintInstance() = default;

    FPhysicsBodyInstance* GetParentBody() const { return ParentBody; }
    void SetParentBody(FPhysicsBodyInstance* InParentBody) { ParentBody = InParentBody; }

    FPhysicsBodyInstance* GetChildBody() const { return ChildBody; }
    void SetChildBody(FPhysicsBodyInstance* InChildBody) { ChildBody = InChildBody; }

    const FPhysicsConstraintDesc& GetConstraintDesc() const { return ConstraintDesc; }
    void SetConstraintDesc(const FPhysicsConstraintDesc& InConstraintDesc) { ConstraintDesc = InConstraintDesc; }

    const FPhysicsJointHandle& GetJointHandle() const { return JointHandle; }
    FPhysicsJointHandle& GetMutableJointHandle() { return JointHandle; }
    void SetJointHandle(const FPhysicsJointHandle& InJointHandle) { JointHandle = InJointHandle; }

    bool IsValidConstraintInstance() const
    {
        return ParentBody != nullptr && ChildBody != nullptr && JointHandle.IsValid();
    }

private:
    FPhysicsBodyInstance*  ParentBody = nullptr;
    FPhysicsBodyInstance*  ChildBody  = nullptr;
    FPhysicsConstraintDesc ConstraintDesc;
    FPhysicsJointHandle    JointHandle;
};
