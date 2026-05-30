#pragma once

#include "PhysicsEventCallback.h"

class UPrimitiveComponent;
class AActor;
struct FHitResult;

/**
 * @file PhysicsSceneInterface.h
 * @brief Physics Runtime의 최상위 Scene 접근 인터페이스 정의.
 *
 * Raycast / SphereSweep은 기존 프로젝트의 Legacy Query API를
 * 유지하면서 EPhysicsCollisionChannel 기반으로 동작한다.
 */
class IPhysicsSceneInterface
{
public:
    virtual ~IPhysicsSceneInterface() = default;

    virtual bool InitializeScene(EPhysicsSceneType SceneType) = 0;
    virtual void ReleaseScene() = 0;

    virtual FPhysicsBodyInstance* CreateBody(
        UPrimitiveComponent* OwnerComponent,
        const FPhysicsBodyDesc& BodyDesc) = 0;
    virtual void DestroyBody(FPhysicsBodyInstance* BodyInstance) = 0;

    virtual FPhysicsConstraintInstance* CreateConstraint(
        FPhysicsBodyInstance* ParentBody,
        FPhysicsBodyInstance* ChildBody,
        const FPhysicsConstraintDesc& ConstraintDesc) = 0;
    virtual void DestroyConstraint(FPhysicsConstraintInstance* ConstraintInstance) = 0;

    virtual void Simulate(const FPhysicsStepInfo& StepInfo) = 0;
    virtual void FetchResults(bool bBlock) = 0;

    virtual void SetEventCallback(IPhysicsEventCallback* InCallback) = 0;
    virtual const FPhysicsRuntimeStats& GetStats() const = 0;

    /** Legacy Query API - EPhysicsCollisionChannel 기반 */
    virtual bool Raycast(
        const FVector& Start,
        const FVector& End,
        FHitResult& OutHit,
        EPhysicsCollisionChannel TraceChannel,
        const AActor* IgnoreActor = nullptr) const = 0;

    virtual bool SphereSweep(
        const FVector& Start,
        const FVector& End,
        float Radius,
        FHitResult& OutHit,
        EPhysicsCollisionChannel TraceChannel,
        const AActor* IgnoreActor = nullptr) const = 0;
};
