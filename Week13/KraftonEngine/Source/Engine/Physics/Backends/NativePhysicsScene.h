#pragma once

#include "Physics/Runtime/PhysicsScene.h"
#include "Core/CollisionTypes.h"
#include "Math/Vector.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ============================================================
// FNativePhysicsScene
//
// hand-written collision math 기반의 물리 Scene.
// O(N^2) brute-force + CollisionMath + 충돌 응답 처리를 담당한다.
// IPhysicsSceneInterface를 통해 PhysX 백엔드와 교체 가능하다.
// ============================================================
class FNativePhysicsScene : public FPhysicsScene
{
public:
    bool InitializeScene(UWorld* InWorld, EPhysicsSceneType SceneType = EPhysicsSceneType::PST_Game) override;
    void ReleaseScene() override;

    FPhysicsBodyInstance* CreateBody(UPrimitiveComponent* OwnerComponent, const FPhysicsBodyDesc& BodyDesc) override;
    FPhysicsBodyInstance* CreateBodyAtTransform(UPrimitiveComponent* OwnerComponent, const FPhysicsBodyDesc& BodyDesc, const FTransform& WorldTransform, bool bSyncOwnerTransform = false) override;
    void DestroyBody(FPhysicsBodyInstance* BodyInstance) override;
    bool GetBodyWorldTransform(const FPhysicsBodyInstance* BodyInstance, FTransform& OutTransform) const override;
    void SetBodyWorldTransform(FPhysicsBodyInstance* BodyInstance, const FTransform& WorldTransform) override;

    FPhysicsConstraintInstance* CreateConstraint(
        FPhysicsBodyInstance* ParentBody,
        FPhysicsBodyInstance* ChildBody,
        const FPhysicsConstraintDesc& ConstraintDesc) override;
    void DestroyConstraint(FPhysicsConstraintInstance* ConstraintInstance) override;

    void RebuildBody(UPrimitiveComponent* Comp) override;
    void Simulate(const FPhysicsStepInfo& StepInfo) override;
    void FetchResults(bool bBlock) override;

    void AddForce(UPrimitiveComponent* Comp, const FVector& Force) override;
    void AddForceAtLocation(UPrimitiveComponent* Comp, const FVector& Force, const FVector& WorldLocation) override;
    void AddTorque(UPrimitiveComponent* Comp, const FVector& Torque) override;

    FVector GetLinearVelocity(UPrimitiveComponent* Comp) const override;
    void SetLinearVelocity(UPrimitiveComponent* Comp, const FVector& Vel) override;
    FVector GetAngularVelocity(UPrimitiveComponent* Comp) const override;
    void SetAngularVelocity(UPrimitiveComponent* Comp, const FVector& Vel) override;

    void SetMass(UPrimitiveComponent* Comp, float Mass) override;
    float GetMass(UPrimitiveComponent* Comp) const override;
    void SetCenterOfMass(UPrimitiveComponent* Comp, const FVector& LocalOffset) override;
    FVector GetCenterOfMass(UPrimitiveComponent* Comp) const override;

    bool Raycast(const FVector& Start, const FVector& End, FHitResult& OutHit,
        ECollisionChannel TraceChannel = ECollisionChannel::ECC_WorldStatic,
        const AActor* IgnoreActor = nullptr) const override;

    bool SphereSweep(const FVector& Start, const FVector& End, float Radius, FHitResult& OutHit,
        ECollisionChannel TraceChannel = ECollisionChannel::ECC_WorldStatic,
        const AActor* IgnoreActor = nullptr) const override;

private:
    bool SphereSweepApprox_ExpandedAABB(const FVector& Start, const FVector& End, float Radius,
        FHitResult& OutHit,
        ECollisionChannel TraceChannel,
        const AActor* IgnoreActor) const;

private:
    UWorld* World = nullptr;
    std::vector<UPrimitiveComponent*> RegisteredComponents;

    struct FBodyState
    {
        FVector Velocity          = { 0, 0, 0 };
        FVector AngularVelocity   = { 0, 0, 0 };
        FVector AccumulatedForce  = { 0, 0, 0 };
        FVector AccumulatedTorque = { 0, 0, 0 };
        float   Mass              = 1.0f;
        FVector CenterOfMassLocal = { 0, 0, 0 };
    };

    std::unordered_map<UPrimitiveComponent*, FBodyState> BodyStates;
    std::unordered_map<UPrimitiveComponent*, FPhysicsBodyInstance*> BodyInstances;

    static constexpr float GravityZ = -9.81f;

    std::unordered_set<FOverlapPair> PreviousOverlaps;
    std::unordered_set<FOverlapPair> CurrentOverlaps;
    std::unordered_set<FOverlapPair> PreviousBlockPairs;
    std::unordered_set<FOverlapPair> CurrentBlockPairs;
};
