#pragma once

#include "PhysicsConstraintInstance.h"
#include "Math/Vector.h"

/** Physics Event 종류 */
enum class EPhysicsEventType : uint8
{
    PET_Contact,
    PET_TriggerEnter,
    PET_TriggerExit,
    PET_ConstraintBreak,
    PET_Wake,
    PET_Sleep
};

/** Contact 지점 정보 */
struct FPhysicsContactPoint
{
    FVector Position   = FVector::ZeroVector;
    FVector Normal     = FVector::UpVector;
    float   Impulse    = 0.0f;
    float   Separation = 0.0f;
};

/** 두 Body 사이의 Contact Event */
struct FPhysicsContactEvent
{
    FPhysicsBodyInstance*         BodyA = nullptr;
    FPhysicsBodyInstance*         BodyB = nullptr;
    TArray<FPhysicsContactPoint>  ContactPoints;
};

/** Constraint Break Event */
struct FPhysicsConstraintBreakEvent
{
    FPhysicsConstraintInstance* Constraint = nullptr;
};

/** Physics Event 수신 인터페이스 */
class IPhysicsEventCallback
{
public:
    virtual ~IPhysicsEventCallback() = default;

    virtual void OnContact(const FPhysicsContactEvent& Event) {}
    virtual void OnTriggerEnter(FPhysicsBodyInstance* TriggerBody, FPhysicsBodyInstance* OtherBody) {}
    virtual void OnTriggerExit(FPhysicsBodyInstance* TriggerBody, FPhysicsBodyInstance* OtherBody) {}
    virtual void OnConstraintBreak(const FPhysicsConstraintBreakEvent& Event) {}
    virtual void OnWake(FPhysicsBodyInstance* Body) {}
    virtual void OnSleep(FPhysicsBodyInstance* Body) {}
};
