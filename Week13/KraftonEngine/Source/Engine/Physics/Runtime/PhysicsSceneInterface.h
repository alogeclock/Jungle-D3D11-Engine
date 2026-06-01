#pragma once

#include "Core/CollisionTypes.h"
#include "Math/Transform.h"
#include "Math/Vector.h"
#include "PhysicsEventCallback.h"

class UWorld;
class UPrimitiveComponent;
class AActor;
struct FHitResult;

/**
 * @file PhysicsSceneInterface.h
 * @brief 모든 물리 백엔드가 공유하는 런타임 Physics Scene 인터페이스.
 *
 * UWorld가 씬 인스턴스를 소유하며, 게임플레이 시스템은 이 인터페이스를 통해
 * 물리 시스템에 접근한다. Native와 PhysX 구현체를 런타임에 교체할 수 있다.
 */
enum class EPhysicsBackend : uint8
{
    Native, // 직접 구현한 충돌 및 시뮬레이션 백엔드
    PhysX,  // NVIDIA PhysX 백엔드
};

/**
 * @brief 백엔드별 Physics Scene의 공통 런타임 인터페이스.
 *
 * 씬 라이프사이클, 강체(Body) 생성, 시뮬레이션 틱,
 * 힘/속도 제어, Raycast/SphereSweep 쿼리를 포함한다.
 */
class IPhysicsSceneInterface
{
public:
    virtual ~IPhysicsSceneInterface() = default;

    // --- 씬 라이프사이클 ---
    virtual bool InitializeScene(UWorld* InWorld, EPhysicsSceneType SceneType = EPhysicsSceneType::PST_Game) = 0;
    virtual void ReleaseScene() = 0;

    // --- Body / Constraint 관리 ---
    virtual FPhysicsBodyInstance* CreateBody(
        UPrimitiveComponent* OwnerComponent,
        const FPhysicsBodyDesc& BodyDesc) = 0;
    virtual FPhysicsBodyInstance* CreateBodyAtTransform(UPrimitiveComponent* OwnerComponent, const FPhysicsBodyDesc& BodyDesc, const FTransform& WorldTransform, bool bSyncOwnerTransform = false) = 0;
    virtual void DestroyBody(FPhysicsBodyInstance* BodyInstance) = 0;
    virtual bool GetBodyWorldTransform(const FPhysicsBodyInstance* BodyInstance, FTransform& OutTransform) const = 0;
    virtual void SetBodyWorldTransform(FPhysicsBodyInstance* BodyInstance, const FTransform& WorldTransform) = 0;

    virtual FPhysicsConstraintInstance* CreateConstraint(
        FPhysicsBodyInstance* ParentBody,
        FPhysicsBodyInstance* ChildBody,
        const FPhysicsConstraintDesc& ConstraintDesc) = 0;
    virtual void DestroyConstraint(FPhysicsConstraintInstance* ConstraintInstance) = 0;

    // 컴포넌트 속성 변경 시 Body 재구성
    virtual void RebuildBody(UPrimitiveComponent* Comp) = 0;

    // --- 시뮬레이션 ---
    virtual void Simulate(const FPhysicsStepInfo& StepInfo) = 0;
    virtual void FetchResults(bool bBlock) = 0;

    // --- 이벤트 / 런타임 통계 ---
    virtual void SetEventCallback(IPhysicsEventCallback* InCallback) = 0;
    virtual const FPhysicsRuntimeStats& GetStats() const = 0;

    // --- 힘 / 토크 ---
    virtual void AddForce(UPrimitiveComponent* Comp, const FVector& Force) = 0;
    virtual void AddForceAtLocation(UPrimitiveComponent* Comp, const FVector& Force, const FVector& WorldLocation) = 0;
    virtual void AddTorque(UPrimitiveComponent* Comp, const FVector& Torque) = 0;

    // --- 속도 ---
    virtual FVector GetLinearVelocity(UPrimitiveComponent* Comp) const = 0;
    virtual void SetLinearVelocity(UPrimitiveComponent* Comp, const FVector& Vel) = 0;
    virtual FVector GetAngularVelocity(UPrimitiveComponent* Comp) const = 0;
    virtual void SetAngularVelocity(UPrimitiveComponent* Comp, const FVector& Vel) = 0;

    // --- 질량 / 무게중심 ---
    virtual void SetMass(UPrimitiveComponent* Comp, float Mass) = 0;
    virtual float GetMass(UPrimitiveComponent* Comp) const = 0;
    virtual void SetCenterOfMass(UPrimitiveComponent* Comp, const FVector& LocalOffset) = 0;
    virtual FVector GetCenterOfMass(UPrimitiveComponent* Comp) const = 0;

    // --- 씬 쿼리 ---
    virtual bool Raycast(
        const FVector& Start,
        const FVector& End,
        FHitResult& OutHit,
        ECollisionChannel TraceChannel = ECollisionChannel::ECC_WorldStatic,
        const AActor* IgnoreActor = nullptr) const = 0;

    virtual bool SphereSweep(
        const FVector& Start,
        const FVector& End,
        float Radius,
        FHitResult& OutHit,
        ECollisionChannel TraceChannel = ECollisionChannel::ECC_WorldStatic,
        const AActor* IgnoreActor = nullptr) const = 0;
};
