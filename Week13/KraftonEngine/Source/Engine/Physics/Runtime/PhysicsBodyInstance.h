/**
 * @file PhysicsBodyInstance.h
 * @brief Component 또는 Bone에 대해 생성되는 Runtime Physics Body 상태를 정의한다.
 */
#pragma once

#include "PhysicsRuntimeTypes.h"

class UPrimitiveComponent;

/** Runtime에서 실제로 생성된 Physics Body 상태 */
class FPhysicsBodyInstance
{
public:
    FPhysicsBodyInstance()  = default;
    ~FPhysicsBodyInstance() = default;

    UPrimitiveComponent* GetOwnerComponent() const { return OwnerComponent; }
    void SetOwnerComponent(UPrimitiveComponent* InOwnerComponent) { OwnerComponent = InOwnerComponent; }

    const FPhysicsBodyDesc& GetBodyDesc() const { return BodyDesc; }
    void SetBodyDesc(const FPhysicsBodyDesc& InBodyDesc) { BodyDesc = InBodyDesc; }

    const FPhysicsActorHandle& GetActorHandle() const { return ActorHandle; }
    FPhysicsActorHandle& GetMutableActorHandle() { return ActorHandle; }
    void SetActorHandle(const FPhysicsActorHandle& InActorHandle) { ActorHandle = InActorHandle; }

    EPhysicsActorState GetActorState() const { return ActorState; }
    void SetActorState(EPhysicsActorState InActorState) { ActorState = InActorState; }

    bool IsValidBodyInstance() const { return ActorHandle.IsValid(); }

private:
    UPrimitiveComponent* OwnerComponent = nullptr;
    FPhysicsBodyDesc     BodyDesc;
    FPhysicsActorHandle  ActorHandle;
    EPhysicsActorState   ActorState = EPhysicsActorState::PAS_None;
};
