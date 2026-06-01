#pragma once

#include "Physics/Runtime/PhysicsScene.h"
#include "Core/CoreTypes.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

class AActor;

namespace physx
{
    class PxFoundation;
    class PxPhysics;
    class PxScene;
    class PxDefaultCpuDispatcher;
    class PxMaterial;
    class PxRigidActor;
    class PxShape;
}

class FPhysXSimulationCallback;

// ============================================================
// FPhysXPhysicsScene
//
// PhysX 4.1 기반 물리 Scene.
// IPhysicsSceneInterface를 통해 Native 백엔드와 교체 가능하다.
// ============================================================
class FPhysXPhysicsScene : public FPhysicsScene
{
public:
    bool InitializeScene(UWorld* InWorld, EPhysicsSceneType SceneType = EPhysicsSceneType::PST_Game) override;
    void ReleaseScene() override;

    FPhysicsBodyInstance* CreateBody(UPrimitiveComponent* OwnerComponent, const FPhysicsBodyDesc& BodyDesc) override;
    void DestroyBody(FPhysicsBodyInstance* BodyInstance) override;

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

    bool Raycast(
        const FVector& Start,
        const FVector& End,
        FHitResult& OutHit,
        ECollisionChannel TraceChannel = ECollisionChannel::ECC_WorldStatic,
        const AActor* IgnoreActor = nullptr) const override;

    bool SphereSweep(
        const FVector& Start,
        const FVector& End,
        float Radius,
        FHitResult& OutHit,
        ECollisionChannel TraceChannel = ECollisionChannel::ECC_WorldStatic,
        const AActor* IgnoreActor = nullptr) const override;

private:
    UWorld* World = nullptr;

    physx::PxFoundation*           Foundation      = nullptr;
    physx::PxPhysics*              Physics         = nullptr;
    physx::PxScene*                Scene           = nullptr;
    physx::PxDefaultCpuDispatcher* Dispatcher      = nullptr;
    physx::PxMaterial*             DefaultMaterial = nullptr;
    FPhysXSimulationCallback*      EventCallback   = nullptr;

    struct FBodyMapping
    {
        AActor*              OwnerActor = nullptr;
        physx::PxRigidActor* Actor      = nullptr;
        UPrimitiveComponent* RootComp   = nullptr;
        TArray<UPrimitiveComponent*> Components;
    };
    std::vector<FBodyMapping> BodyMappings;
    std::unordered_map<UPrimitiveComponent*, FPhysicsBodyInstance*> BodyInstances;

    FBodyMapping* FindMappingByActor(AActor* OwnerActor);
    const FBodyMapping* FindMappingByActor(AActor* OwnerActor) const;
    FBodyMapping* FindMappingByComponent(UPrimitiveComponent* Comp);
    const FBodyMapping* FindMappingByComponent(UPrimitiveComponent* Comp) const;

    physx::PxShape* AddShapeForComponent(FBodyMapping& Mapping, UPrimitiveComponent* Comp);
    void DetachShapeForComponent(FBodyMapping& Mapping, UPrimitiveComponent* Comp);

    void RegisterComponentInternal(UPrimitiveComponent* Comp);
    void UnregisterComponentInternal(UPrimitiveComponent* Comp);

    void WaitForSimulation();
    void ExecuteOrDeferSceneWrite(std::function<void()> Command);
    void FlushDeferredSceneCommands();
    void SyncEngineTransformsToPhysX();
    void SyncPhysXTransformsToEngine();

    std::atomic_bool bSimulationInFlight{ false };
    std::mutex DeferredSceneCommandMutex;
    std::vector<std::function<void()>> DeferredSceneCommands;
};
