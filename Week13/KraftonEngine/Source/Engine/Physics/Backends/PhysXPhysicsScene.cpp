#include "Physics/Backends/PhysXPhysicsScene.h"
#include "Component/PrimitiveComponent.h"
#include "Component/BoxComponent.h"
#include "Component/SphereComponent.h"
#include "Component/CapsuleComponent.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Math/Quat.h"
#include "Object/Object.h"  // IsAliveObject
#include "Core/Log.h"

// PhysX headers
#include <PxPhysicsAPI.h>

using namespace physx;

// ============================================================
// PhysX Error Callback
// ============================================================
class FPhysXErrorCallback : public PxErrorCallback
{
public:
    void reportError(PxErrorCode::Enum code, const char* message,
        const char* file, int line) override
    {
        const char* severity = "Info";
        if (code == PxErrorCode::eABORT || code == PxErrorCode::eOUT_OF_MEMORY)
            severity = "Fatal";
        else if (code == PxErrorCode::eINTERNAL_ERROR || code == PxErrorCode::eINVALID_OPERATION)
            severity = "Error";
        else if (code == PxErrorCode::eINVALID_PARAMETER || code == PxErrorCode::ePERF_WARNING)
            severity = "Warning";
        else if (code == PxErrorCode::eDEBUG_WARNING)
            severity = "Warning";
        UE_LOG("[PhysX %s] %s (%s:%d)", severity, message, file, line);
    }
};

static FPhysXErrorCallback GPhysXErrorCallback;
static PxDefaultAllocator  GPhysXAllocator;

// ============================================================
// PhysX Foundation/Physics 싱글턴
// ============================================================
static PxFoundation* GSharedFoundation = nullptr;
static PxPhysics*    GSharedPhysics    = nullptr;
static int32         GSharedRefCount   = 0;

static void AcquireSharedPhysX(PxFoundation*& OutFoundation, PxPhysics*& OutPhysics)
{
    if (GSharedRefCount == 0)
    {
        GSharedFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, GPhysXAllocator, GPhysXErrorCallback);
        if (GSharedFoundation)
            GSharedPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *GSharedFoundation, PxTolerancesScale());
    }
    ++GSharedRefCount;
    OutFoundation = GSharedFoundation;
    OutPhysics    = GSharedPhysics;
}

static void ReleaseSharedPhysX()
{
    if (--GSharedRefCount <= 0)
    {
        if (GSharedPhysics)    { GSharedPhysics->release();    GSharedPhysics    = nullptr; }
        if (GSharedFoundation) { GSharedFoundation->release(); GSharedFoundation = nullptr; }
        GSharedRefCount = 0;
    }
}

// ============================================================
// PhysX Simulation Event Callback
// ============================================================
class FPhysXSimulationCallback : public PxSimulationEventCallback
{
public:
    struct FQueuedHit
    {
        UPrimitiveComponent* Self  = nullptr;
        UPrimitiveComponent* Other = nullptr;
        FVector NormalImpulse{0,0,0};
        FHitResult Hit;
        bool bBegin = true;
    };
    struct FQueuedTrigger
    {
        UPrimitiveComponent* Self  = nullptr;
        UPrimitiveComponent* Other = nullptr;
        bool bBegin = true;
    };

    void onContact(const PxContactPairHeader& PairHeader,
        const PxContactPair* Pairs, PxU32 Count) override
    {
        if (PairHeader.flags & PxContactPairHeaderFlag::eREMOVED_ACTOR_0
            || PairHeader.flags & PxContactPairHeaderFlag::eREMOVED_ACTOR_1)
            return;

        for (PxU32 i = 0; i < Count; ++i)
        {
            const PxContactPair& CP = Pairs[i];
            const bool bBegin = CP.events.isSet(PxPairFlag::eNOTIFY_TOUCH_FOUND);
            const bool bEnd   = CP.events.isSet(PxPairFlag::eNOTIFY_TOUCH_LOST);
            if (!bBegin && !bEnd) continue;

            auto* CompA = CP.shapes[0] ? static_cast<UPrimitiveComponent*>(CP.shapes[0]->userData) : nullptr;
            auto* CompB = CP.shapes[1] ? static_cast<UPrimitiveComponent*>(CP.shapes[1]->userData) : nullptr;
            if (!CompA || !CompB) continue;

            if (bEnd)
            {
                PendingHits.push_back({ CompA, CompB, {}, {}, false });
                PendingHits.push_back({ CompB, CompA, {}, {}, false });
                continue;
            }

            PxContactPairPoint ContactPoints[1];
            PxU32 NumPoints = CP.extractContacts(ContactPoints, 1);

            FVector ContactPos(0,0,0), ContactNormal(0,0,1);
            float Penetration = 0.0f;
            if (NumPoints > 0)
            {
                ContactPos    = FVector(ContactPoints[0].position.x, ContactPoints[0].position.y, ContactPoints[0].position.z);
                ContactNormal = FVector(ContactPoints[0].normal.x,   ContactPoints[0].normal.y,   ContactPoints[0].normal.z);
                Penetration   = ContactPoints[0].separation;
            }

            const FVector NI = ContactNormal * Penetration;

            FQueuedHit A;
            A.Self = CompA; A.Other = CompB; A.NormalImpulse = NI;
            A.Hit.bHit = true; A.Hit.HitComponent = CompB; A.Hit.HitActor = CompB->GetOwner();
            A.Hit.WorldHitLocation = ContactPos; A.Hit.ImpactNormal = ContactNormal;
            A.Hit.WorldNormal = ContactNormal; A.Hit.PenetrationDepth = -Penetration;
            PendingHits.push_back(A);

            FQueuedHit B;
            B.Self = CompB; B.Other = CompA; B.NormalImpulse = NI * -1.0f;
            B.Hit.bHit = true; B.Hit.HitComponent = CompA; B.Hit.HitActor = CompA->GetOwner();
            B.Hit.WorldHitLocation = ContactPos; B.Hit.ImpactNormal = ContactNormal * -1.0f;
            B.Hit.WorldNormal = ContactNormal * -1.0f; B.Hit.PenetrationDepth = -Penetration;
            PendingHits.push_back(B);
        }
    }

    void onTrigger(PxTriggerPair* Pairs, PxU32 Count) override
    {
        for (PxU32 i = 0; i < Count; ++i)
        {
            const PxTriggerPair& TP = Pairs[i];
            if (TP.flags & (PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER | PxTriggerPairFlag::eREMOVED_SHAPE_OTHER))
                continue;
            auto* TriggerComp = TP.triggerShape ? static_cast<UPrimitiveComponent*>(TP.triggerShape->userData) : nullptr;
            auto* OtherComp   = TP.otherShape   ? static_cast<UPrimitiveComponent*>(TP.otherShape->userData)   : nullptr;
            if (!TriggerComp || !OtherComp) continue;
            const bool bBegin = (TP.status == PxPairFlag::eNOTIFY_TOUCH_FOUND);
            const bool bEnd   = (TP.status == PxPairFlag::eNOTIFY_TOUCH_LOST);
            if (!bBegin && !bEnd) continue;
            if (TriggerComp->GetGenerateOverlapEvents()) PendingTriggers.push_back({ TriggerComp, OtherComp, bBegin });
            if (OtherComp->GetGenerateOverlapEvents())   PendingTriggers.push_back({ OtherComp, TriggerComp, bBegin });
        }
    }

    void DispatchPendingEvents()
    {
        std::vector<FQueuedHit>     Hits;    Hits.swap(PendingHits);
        std::vector<FQueuedTrigger> Trigs;   Trigs.swap(PendingTriggers);

        for (FQueuedHit& E : Hits)
        {
            if (!IsAliveObject(E.Self) || !IsAliveObject(E.Other)) continue;
            AActor* OtherActor = E.Other->GetOwner();
            if (E.bBegin) E.Self->NotifyComponentHit(E.Self, OtherActor, E.Other, E.NormalImpulse, E.Hit);
            else          E.Self->NotifyComponentEndHit(E.Self, OtherActor, E.Other);
        }
        for (FQueuedTrigger& E : Trigs)
        {
            if (!IsAliveObject(E.Self) || !IsAliveObject(E.Other)) continue;
            AActor* OtherActor = E.Other->GetOwner();
            if (E.bBegin) { FHitResult D; E.Self->NotifyComponentBeginOverlap(E.Self, OtherActor, E.Other, 0, false, D); }
            else            E.Self->NotifyComponentEndOverlap(E.Self, OtherActor, E.Other, 0);
        }
    }

    void onConstraintBreak(PxConstraintInfo*, PxU32) override {}
    void onWake(PxActor**, PxU32) override {}
    void onSleep(PxActor**, PxU32) override {}
    void onAdvance(const PxRigidBody* const*, const PxTransform*, const PxU32) override {}

private:
    std::vector<FQueuedHit>     PendingHits;
    std::vector<FQueuedTrigger> PendingTriggers;
};

// ============================================================
// Transform 변환 유틸
// ============================================================
static PxVec3 ToPxVec3(const FVector& V) { return PxVec3(V.X, V.Y, V.Z); }
static PxQuat ToPxQuat(const FQuat& Q)   { return PxQuat(Q.X, Q.Y, Q.Z, Q.W); }
static FVector ToFVector(const PxVec3& V) { return FVector(V.x, V.y, V.z); }
static FQuat   ToFQuat  (const PxQuat& Q) { return FQuat(Q.x, Q.y, Q.z, Q.w); }

static PxTransform GetPxTransform(UPrimitiveComponent* Comp)
{
    return PxTransform(ToPxVec3(Comp->GetWorldLocation()), ToPxQuat(Comp->GetWorldMatrix().ToQuat()));
}

static void ApplyRootMassAndCOM(PxRigidDynamic* Dyn, UPrimitiveComponent* Root)
{
    if (!Dyn || !Root) return;
    const float MassKg = (Root->GetMass() > 0.0f) ? Root->GetMass() : 1.0f;
    PxRigidBodyExt::setMassAndUpdateInertia(*Dyn, MassKg);
    Dyn->setCMassLocalPose(PxTransform(ToPxVec3(Root->GetCenterOfMass())));
}

// ============================================================
// Collision Filtering
// ============================================================
static void SetupFilterData(PxShape* Shape, UPrimitiveComponent* Comp)
{
    PxFilterData Filter;
    Filter.word0 = static_cast<PxU32>(Comp->GetCollisionObjectType());
    Filter.word1 = 0; Filter.word2 = 0;
    Filter.word3 = Comp->GetOwner() ? Comp->GetOwner()->GetUUID() : 0;

    for (int32 Ch = 0; Ch < NumActiveCollisionChannels; ++Ch)
    {
        ECollisionResponse R = Comp->GetCollisionResponseToChannel(static_cast<ECollisionChannel>(Ch));
        if (R == ECollisionResponse::Block)   Filter.word1 |= (1u << Ch);
        if (R == ECollisionResponse::Overlap) Filter.word2 |= (1u << Ch);
    }
    Shape->setSimulationFilterData(Filter);
    Shape->setQueryFilterData(Filter);
}

static PxFilterFlags KraftonFilterShader(
    PxFilterObjectAttributes attributes0, PxFilterData filterData0,
    PxFilterObjectAttributes attributes1, PxFilterData filterData1,
    PxPairFlags& pairFlags, const void*, PxU32)
{
    if (filterData0.word3 != 0 && filterData0.word3 == filterData1.word3)
        return PxFilterFlag::eKILL;

    if (PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1))
    {
        pairFlags = PxPairFlag::eTRIGGER_DEFAULT;
        return PxFilterFlag::eDEFAULT;
    }

    PxU32 channelA = filterData0.word0, channelB = filterData1.word0;
    bool bABlocksB = (filterData0.word1 & (1u << channelB)) != 0;
    bool bBBlocksA = (filterData1.word1 & (1u << channelA)) != 0;

    if (bABlocksB && bBBlocksA)
    {
        pairFlags = PxPairFlag::eCONTACT_DEFAULT
            | PxPairFlag::eNOTIFY_TOUCH_FOUND
            | PxPairFlag::eNOTIFY_TOUCH_LOST
            | PxPairFlag::eNOTIFY_CONTACT_POINTS;
        return PxFilterFlag::eDEFAULT;
    }

    bool bAOverlapsB = (filterData0.word2 & (1u << channelB)) != 0;
    bool bBOverlapsA = (filterData1.word2 & (1u << channelA)) != 0;
    if (bAOverlapsB || bBOverlapsA)
    {
        pairFlags = PxPairFlag::eDETECT_DISCRETE_CONTACT
            | PxPairFlag::eNOTIFY_TOUCH_FOUND
            | PxPairFlag::eNOTIFY_TOUCH_LOST;
        return PxFilterFlag::eDEFAULT;
    }
    return PxFilterFlag::eKILL;
}

// ============================================================
// Lifecycle
// ============================================================

bool FPhysXPhysicsScene::InitializeScene(UWorld* InWorld, EPhysicsSceneType SceneType)
{
    World = InWorld;
    SetSceneType(SceneType);
    AcquireSharedPhysX(Foundation, Physics);
    if (!Foundation || !Physics)
    {
        UE_LOG("[PhysX] Failed to create Foundation or Physics");
        return false;
    }

    Dispatcher    = PxDefaultCpuDispatcherCreate(2);
    EventCallback = new FPhysXSimulationCallback();

    PxSceneDesc SceneDesc(Physics->getTolerancesScale());
    SceneDesc.gravity                  = PxVec3(0.0f, 0.0f, -9.81f);
    SceneDesc.cpuDispatcher            = Dispatcher;
    SceneDesc.filterShader             = KraftonFilterShader;
    SceneDesc.simulationEventCallback  = EventCallback;
    Scene = Physics->createScene(SceneDesc);
    if (!Scene)
    {
        UE_LOG("[PhysX] Failed to create Scene");
        return false;
    }
    SetNativeSceneHandle(Scene);
    DefaultMaterial = Physics->createMaterial(0.5f, 0.5f, 0.3f);
    UE_LOG("[PhysX] Initialized successfully (Scene=%p)", Scene);
    return true;
}

void FPhysXPhysicsScene::ReleaseScene()
{
    for (auto& Pair : BodyInstances) delete Pair.second;
    BodyInstances.clear();

    for (auto& Mapping : BodyMappings)
    {
        if (Mapping.Actor) { Mapping.Actor->release(); Mapping.Actor = nullptr; }
    }
    BodyMappings.clear();

    if (DefaultMaterial) { DefaultMaterial->release(); DefaultMaterial = nullptr; }
    if (Scene)           { Scene->release();           Scene           = nullptr; }
    if (EventCallback)   { delete EventCallback;       EventCallback   = nullptr; }
    if (Dispatcher)      { Dispatcher->release();      Dispatcher      = nullptr; }

    Foundation = nullptr;
    Physics    = nullptr;
    ReleaseSharedPhysX();
    ClearRegisteredInstances();
    SetNativeSceneHandle(nullptr);
    World = nullptr;
}

// ============================================================
// Body 관리
// ============================================================

FPhysicsBodyInstance* FPhysXPhysicsScene::CreateBody(UPrimitiveComponent* Comp, const FPhysicsBodyDesc& BodyDesc)
{
    if (!Comp) return nullptr;

    // 이미 등록된 경우 기존 인스턴스 반환
    auto ExistingIt = BodyInstances.find(Comp);
    if (ExistingIt != BodyInstances.end()) return ExistingIt->second;

    RegisterComponentInternal(Comp);

    FPhysicsBodyInstance* Instance = new FPhysicsBodyInstance();
    Instance->SetOwnerComponent(Comp);
    Instance->SetBodyDesc(BodyDesc);

    // 매핑에서 PxRigidActor 핸들 연결
    FBodyMapping* Mapping = FindMappingByComponent(Comp);
    if (Mapping && Mapping->Actor)
    {
        FPhysicsActorHandle Handle;
        Handle.NativeActor = static_cast<void*>(Mapping->Actor);
        Instance->SetActorHandle(Handle);
        Instance->SetActorState(EPhysicsActorState::PAS_Added);
    }

    BodyInstances[Comp] = Instance;
    RegisterBodyInstance(Instance);
    GetMutableRuntimeStats().BodyCount = static_cast<int32>(BodyInstances.size());
    return Instance;
}

void FPhysXPhysicsScene::DestroyBody(FPhysicsBodyInstance* BodyInstance)
{
    if (!BodyInstance) return;
    UPrimitiveComponent* Comp = BodyInstance->GetOwnerComponent();
    if (!Comp) return;

    UnregisterComponentInternal(Comp);
    BodyInstances.erase(Comp);
    UnregisterBodyInstance(BodyInstance);

    BodyInstance->SetActorState(EPhysicsActorState::PAS_Destroyed);
    BodyInstance->GetMutableActorHandle().Reset();
    delete BodyInstance;

    GetMutableRuntimeStats().BodyCount = static_cast<int32>(BodyInstances.size());
}

FPhysicsConstraintInstance* FPhysXPhysicsScene::CreateConstraint(
    FPhysicsBodyInstance* /*ParentBody*/,
    FPhysicsBodyInstance* /*ChildBody*/,
    const FPhysicsConstraintDesc& /*ConstraintDesc*/)
{
    return nullptr; // 향후 PxJoint 구현 예정
}

void FPhysXPhysicsScene::DestroyConstraint(FPhysicsConstraintInstance* /*ConstraintInstance*/)
{
    // noop
}

// ============================================================
// Internal Register/Unregister (기존 RegisterComponent 로직)
// ============================================================

void FPhysXPhysicsScene::RegisterComponentInternal(UPrimitiveComponent* Comp)
{
    if (!Comp || !Scene || !Physics || !DefaultMaterial) return;
    if (FindMappingByComponent(Comp)) return;

    AActor* OwnerActor = Comp->GetOwner();
    if (!OwnerActor) return;

    FBodyMapping* Mapping = FindMappingByActor(OwnerActor);
    if (!Mapping)
    {
        UPrimitiveComponent* RootPrim = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent());
        if (!RootPrim) RootPrim = Comp;

        const bool bDynamic = RootPrim->GetSimulatePhysics();
        PxTransform BodyXf  = GetPxTransform(RootPrim);

        PxRigidActor* Body = bDynamic
            ? static_cast<PxRigidActor*>(Physics->createRigidDynamic(BodyXf))
            : static_cast<PxRigidActor*>(Physics->createRigidStatic(BodyXf));
        if (!Body) return;

        Body->userData = OwnerActor;
        Scene->addActor(*Body);

        FBodyMapping NewMapping;
        NewMapping.OwnerActor = OwnerActor;
        NewMapping.Actor      = Body;
        NewMapping.RootComp   = RootPrim;
        BodyMappings.push_back(NewMapping);
        Mapping = &BodyMappings.back();
    }

    PxShape* Shape = AddShapeForComponent(*Mapping, Comp);
    if (!Shape) return;
    Mapping->Components.push_back(Comp);

    if (PxRigidDynamic* Dyn = Mapping->Actor->is<PxRigidDynamic>())
        ApplyRootMassAndCOM(Dyn, Mapping->RootComp);
}

void FPhysXPhysicsScene::UnregisterComponentInternal(UPrimitiveComponent* Comp)
{
    if (!Comp || !Scene) return;

    FBodyMapping* Mapping = FindMappingByComponent(Comp);
    if (!Mapping) return;

    DetachShapeForComponent(*Mapping, Comp);
    Mapping->Components.erase(
        std::remove(Mapping->Components.begin(), Mapping->Components.end(), Comp),
        Mapping->Components.end());

    if (Mapping->Components.empty())
    {
        if (Mapping->Actor) { Scene->removeActor(*Mapping->Actor); Mapping->Actor->release(); }
        *Mapping = BodyMappings.back();
        BodyMappings.pop_back();
        return;
    }
    if (PxRigidDynamic* Dyn = Mapping->Actor->is<PxRigidDynamic>())
        ApplyRootMassAndCOM(Dyn, Mapping->RootComp);
}

void FPhysXPhysicsScene::RebuildBody(UPrimitiveComponent* Comp)
{
    if (!Comp || !Scene) return;
    AActor* OwnerActor = Comp->GetOwner();
    if (!OwnerActor) return;

    FBodyMapping* Mapping = FindMappingByActor(OwnerActor);
    if (!Mapping) return;

    TArray<UPrimitiveComponent*> CompList = Mapping->Components;
    for (UPrimitiveComponent* C : CompList) UnregisterComponentInternal(C);
    for (UPrimitiveComponent* C : CompList) RegisterComponentInternal(C);

    // BodyInstance 핸들 갱신
    for (UPrimitiveComponent* C : CompList)
    {
        auto It = BodyInstances.find(C);
        if (It == BodyInstances.end()) continue;
        FBodyMapping* NewMapping = FindMappingByComponent(C);
        if (NewMapping && NewMapping->Actor)
        {
            FPhysicsActorHandle Handle;
            Handle.NativeActor = static_cast<void*>(NewMapping->Actor);
            It->second->SetActorHandle(Handle);
        }
    }
}

// ============================================================
// 시뮬레이션 (구 Tick 분리)
// ============================================================

void FPhysXPhysicsScene::Simulate(const FPhysicsStepInfo& StepInfo)
{
    if (!Scene) return;
    float DeltaTime = StepInfo.DeltaTime;
    if (DeltaTime <= 0.0f) return;

    constexpr float MaxPhysicsDeltaTime = 0.1f;
    if (DeltaTime > MaxPhysicsDeltaTime) DeltaTime = MaxPhysicsDeltaTime;

    // Pre-simulate: Engine → PhysX Transform 동기화
    constexpr float TeleportPosThresholdSq = 1.0f;
    constexpr float TeleportRotThreshold   = 0.99f;

    for (auto& Mapping : BodyMappings)
    {
        if (!Mapping.RootComp || !Mapping.Actor) continue;
        PxTransform NewPose = GetPxTransform(Mapping.RootComp);

        if (PxRigidDynamic* Dynamic = Mapping.Actor->is<PxRigidDynamic>())
        {
            if (Dynamic->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC)
            {
                Dynamic->setKinematicTarget(NewPose);
            }
            else
            {
                PxTransform PxPose = Dynamic->getGlobalPose();
                PxVec3 dp = NewPose.p - PxPose.p;
                const float DistSq = dp.x*dp.x + dp.y*dp.y + dp.z*dp.z;
                const float QDot = std::abs(
                    NewPose.q.x*PxPose.q.x + NewPose.q.y*PxPose.q.y +
                    NewPose.q.z*PxPose.q.z + NewPose.q.w*PxPose.q.w);
                if (DistSq > TeleportPosThresholdSq || QDot < TeleportRotThreshold)
                    Dynamic->setGlobalPose(NewPose);
            }
        }
        else if (Mapping.Actor->is<PxRigidStatic>())
        {
            Mapping.Actor->setGlobalPose(NewPose);
        }
    }

    Scene->simulate(DeltaTime);
}

void FPhysXPhysicsScene::FetchResults(bool bBlock)
{
    if (!Scene) return;
    Scene->fetchResults(bBlock);

    // Post-simulate: PhysX → Engine Transform 동기화
    for (auto& Mapping : BodyMappings)
    {
        if (!Mapping.RootComp || !Mapping.Actor) continue;
        PxRigidDynamic* Dynamic = Mapping.Actor->is<PxRigidDynamic>();
        if (!Dynamic) continue;
        if (Dynamic->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC) continue;
        if (Dynamic->isSleeping()) continue;

        PxTransform Pose = Dynamic->getGlobalPose();
        Mapping.RootComp->SetWorldLocation(ToFVector(Pose.p));
        Mapping.RootComp->SetRelativeRotation(ToFQuat(Pose.q));
    }

    // Dispatch deferred contact/trigger events
    if (EventCallback) EventCallback->DispatchPendingEvents();

    GetMutableRuntimeStats().ContactCount = 0; // PhysX 측 contact 수는 콜백에서 집계 가능 (현재 생략)
}

// ============================================================
// AddShapeForComponent / DetachShapeForComponent
// ============================================================

PxShape* FPhysXPhysicsScene::AddShapeForComponent(FBodyMapping& Mapping, UPrimitiveComponent* Comp)
{
    if (!Mapping.Actor || !DefaultMaterial || !Comp) return nullptr;

    PxGeometryHolder Geom;
    bool bHasGeom = false;
    PxQuat ShapeAxisRot = PxQuat(PxIdentity);

    if (auto* Box = Cast<UBoxComponent>(Comp))
    {
        FVector Ext = Box->GetScaledBoxExtent();
        Geom = PxBoxGeometry(Ext.X, Ext.Y, Ext.Z);
        bHasGeom = true;
    }
    else if (auto* Sphere = Cast<USphereComponent>(Comp))
    {
        Geom = PxSphereGeometry(Sphere->GetScaledSphereRadius());
        bHasGeom = true;
    }
    else if (auto* Capsule = Cast<UCapsuleComponent>(Comp))
    {
        float Radius = Capsule->GetScaledCapsuleRadius();
        float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
        Geom = PxCapsuleGeometry(Radius, HalfHeight - Radius);
        ShapeAxisRot = PxQuat(PxHalfPi, PxVec3(0.0f, 0.0f, 1.0f));
        bHasGeom = true;
    }
    if (!bHasGeom) return nullptr;

    PxShape* Shape = PxRigidActorExt::createExclusiveShape(*Mapping.Actor, Geom.any(), *DefaultMaterial);
    if (!Shape) return nullptr;

    PxTransform LocalPose = PxTransform(PxIdentity);
    if (Comp != Mapping.RootComp && Mapping.RootComp)
    {
        FVector RootPos = Mapping.RootComp->GetWorldLocation();
        FQuat   RootRot = Mapping.RootComp->GetWorldMatrix().ToQuat();
        FVector CompPos = Comp->GetWorldLocation();
        FQuat   CompRot = Comp->GetWorldMatrix().ToQuat();

        FQuat InvRootRot = RootRot.Inverse();
        FVector LocalPos = InvRootRot.RotateVector(CompPos - RootPos);
        FQuat   LocalRot = InvRootRot * CompRot;
        LocalPose = PxTransform(ToPxVec3(LocalPos), ToPxQuat(LocalRot));
    }
    LocalPose.q = LocalPose.q * ShapeAxisRot;
    Shape->setLocalPose(LocalPose);

    SetupFilterData(Shape, Comp);

    bool bShouldBeTrigger = Comp->GetGenerateOverlapEvents();
    if (!bShouldBeTrigger)
    {
        bool bHasAnyBlockResponse = false;
        for (int32 Ch = 0; Ch < NumActiveCollisionChannels; ++Ch)
        {
            if (Comp->GetCollisionResponseToChannel(static_cast<ECollisionChannel>(Ch)) == ECollisionResponse::Block)
            { bHasAnyBlockResponse = true; break; }
        }
        bShouldBeTrigger = !bHasAnyBlockResponse;
    }
    if (bShouldBeTrigger)
    {
        Shape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
        Shape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true);
    }

    Shape->userData = Comp;
    return Shape;
}

void FPhysXPhysicsScene::DetachShapeForComponent(FBodyMapping& Mapping, UPrimitiveComponent* Comp)
{
    if (!Mapping.Actor || !Comp) return;
    const PxU32 NumShapes = Mapping.Actor->getNbShapes();
    if (NumShapes == 0) return;

    std::vector<PxShape*> Shapes(NumShapes);
    Mapping.Actor->getShapes(Shapes.data(), NumShapes);
    for (PxShape* Shape : Shapes)
    {
        if (Shape && Shape->userData == Comp)
        { Mapping.Actor->detachShape(*Shape); break; }
    }
}

// ============================================================
// Mapping helpers
// ============================================================

FPhysXPhysicsScene::FBodyMapping* FPhysXPhysicsScene::FindMappingByActor(AActor* OwnerActor)
{
    for (auto& M : BodyMappings) if (M.OwnerActor == OwnerActor) return &M;
    return nullptr;
}
const FPhysXPhysicsScene::FBodyMapping* FPhysXPhysicsScene::FindMappingByActor(AActor* OwnerActor) const
{
    for (const auto& M : BodyMappings) if (M.OwnerActor == OwnerActor) return &M;
    return nullptr;
}
FPhysXPhysicsScene::FBodyMapping* FPhysXPhysicsScene::FindMappingByComponent(UPrimitiveComponent* Comp)
{
    if (!Comp) return nullptr;
    for (auto& M : BodyMappings) for (UPrimitiveComponent* C : M.Components) if (C == Comp) return &M;
    return nullptr;
}
const FPhysXPhysicsScene::FBodyMapping* FPhysXPhysicsScene::FindMappingByComponent(UPrimitiveComponent* Comp) const
{
    if (!Comp) return nullptr;
    for (const auto& M : BodyMappings) for (UPrimitiveComponent* C : M.Components) if (C == Comp) return &M;
    return nullptr;
}

// ============================================================
// Force / Torque
// ============================================================

void FPhysXPhysicsScene::AddForce(UPrimitiveComponent* Comp, const FVector& Force)
{
    FBodyMapping* M = FindMappingByComponent(Comp);
    if (!M || !M->Actor) return;
    PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
    if (Dyn) Dyn->addForce(ToPxVec3(Force));
}
void FPhysXPhysicsScene::AddForceAtLocation(UPrimitiveComponent* Comp, const FVector& Force, const FVector& WorldLocation)
{
    FBodyMapping* M = FindMappingByComponent(Comp);
    if (!M || !M->Actor) return;
    PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
    if (Dyn) PxRigidBodyExt::addForceAtPos(*Dyn, ToPxVec3(Force), ToPxVec3(WorldLocation));
}
void FPhysXPhysicsScene::AddTorque(UPrimitiveComponent* Comp, const FVector& Torque)
{
    FBodyMapping* M = FindMappingByComponent(Comp);
    if (!M || !M->Actor) return;
    PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
    if (Dyn) Dyn->addTorque(ToPxVec3(Torque));
}

// ============================================================
// Velocity
// ============================================================

FVector FPhysXPhysicsScene::GetLinearVelocity(UPrimitiveComponent* Comp) const
{
    const FBodyMapping* M = FindMappingByComponent(Comp);
    if (!M || !M->Actor) return { 0, 0, 0 };
    PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
    return Dyn ? ToFVector(Dyn->getLinearVelocity()) : FVector(0, 0, 0);
}
void FPhysXPhysicsScene::SetLinearVelocity(UPrimitiveComponent* Comp, const FVector& Vel)
{
    FBodyMapping* M = FindMappingByComponent(Comp);
    if (!M || !M->Actor) return;
    PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
    if (Dyn) Dyn->setLinearVelocity(ToPxVec3(Vel));
}
FVector FPhysXPhysicsScene::GetAngularVelocity(UPrimitiveComponent* Comp) const
{
    const FBodyMapping* M = FindMappingByComponent(Comp);
    if (!M || !M->Actor) return { 0, 0, 0 };
    PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
    return Dyn ? ToFVector(Dyn->getAngularVelocity()) : FVector(0, 0, 0);
}
void FPhysXPhysicsScene::SetAngularVelocity(UPrimitiveComponent* Comp, const FVector& Vel)
{
    FBodyMapping* M = FindMappingByComponent(Comp);
    if (!M || !M->Actor) return;
    PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
    if (Dyn) Dyn->setAngularVelocity(ToPxVec3(Vel));
}

// ============================================================
// Mass
// ============================================================

void FPhysXPhysicsScene::SetMass(UPrimitiveComponent* Comp, float NewMass)
{
    FBodyMapping* M = FindMappingByComponent(Comp);
    if (!M || !M->Actor) return;
    PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
    if (!Dyn) return;
    PxVec3 LocalCOM = M->RootComp ? ToPxVec3(M->RootComp->GetCenterOfMass()) : PxVec3(0);
    PxRigidBodyExt::setMassAndUpdateInertia(*Dyn, NewMass, &LocalCOM);
}
float FPhysXPhysicsScene::GetMass(UPrimitiveComponent* Comp) const
{
    const FBodyMapping* M = FindMappingByComponent(Comp);
    if (!M || !M->Actor) return 1.0f;
    PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
    return Dyn ? Dyn->getMass() : 1.0f;
}
void FPhysXPhysicsScene::SetCenterOfMass(UPrimitiveComponent* Comp, const FVector& LocalOffset)
{
    FBodyMapping* M = FindMappingByComponent(Comp);
    if (!M || !M->Actor) return;
    PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
    if (Dyn) Dyn->setCMassLocalPose(PxTransform(ToPxVec3(LocalOffset)));
}
FVector FPhysXPhysicsScene::GetCenterOfMass(UPrimitiveComponent* Comp) const
{
    const FBodyMapping* M = FindMappingByComponent(Comp);
    if (!M || !M->Actor) return { 0, 0, 0 };
    PxRigidDynamic* Dyn = M->Actor->is<PxRigidDynamic>();
    return Dyn ? ToFVector(Dyn->getCMassLocalPose().p) : FVector(0, 0, 0);
}

// ============================================================
// Raycast (Start + End 시그니처)
// ============================================================

bool FPhysXPhysicsScene::Raycast(const FVector& Start, const FVector& End, FHitResult& OutHit,
    ECollisionChannel TraceChannel, const AActor* IgnoreActor) const
{
    if (!Scene) return false;

    FVector Move = End - Start;
    float MaxDist2 = Move.X*Move.X + Move.Y*Move.Y + Move.Z*Move.Z;
    if (MaxDist2 < 1e-12f) return false;
    const float MaxDist = sqrtf(MaxDist2);
    const FVector Dir(Move.X/MaxDist, Move.Y/MaxDist, Move.Z/MaxDist);

    struct FChannelRaycastFilter : PxQueryFilterCallback
    {
        const AActor* IgnoreActor = nullptr;
        PxU32 TraceBit = 0;
        FChannelRaycastFilter(const AActor* A, ECollisionChannel Ch)
            : IgnoreActor(A), TraceBit(1u << static_cast<PxU32>(Ch)) {}
        PxQueryHitType::Enum preFilter(const PxFilterData&, const PxShape* Shape,
            const PxRigidActor* Actor, PxHitFlags&) override
        {
            if (IgnoreActor && Actor && Actor->userData == IgnoreActor) return PxQueryHitType::eNONE;
            if (Shape && (Shape->getQueryFilterData().word1 & TraceBit) == 0) return PxQueryHitType::eNONE;
            return PxQueryHitType::eBLOCK;
        }
        PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&) override
        { return PxQueryHitType::eBLOCK; }
    };

    PxRaycastBuffer Hit;
    PxQueryFilterData FilterData;
    FilterData.flags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER;
    FChannelRaycastFilter FilterCallback(IgnoreActor, TraceChannel);

    bool bStatus = Scene->raycast(ToPxVec3(Start), ToPxVec3(Dir), MaxDist,
        Hit, PxHitFlag::eDEFAULT, FilterData, &FilterCallback);
    if (!bStatus || !Hit.hasBlock) return false;

    const PxRaycastHit& Block = Hit.block;
    OutHit.bHit            = true;
    OutHit.Distance        = Block.distance;
    OutHit.WorldHitLocation= ToFVector(Block.position);
    OutHit.ShapeLocation   = OutHit.WorldHitLocation;
    OutHit.ImpactNormal    = ToFVector(Block.normal);
    OutHit.WorldNormal     = OutHit.ImpactNormal;

    if (Block.shape && Block.shape->userData)
    {
        OutHit.HitComponent = static_cast<UPrimitiveComponent*>(Block.shape->userData);
        OutHit.HitActor     = OutHit.HitComponent->GetOwner();
    }
    else if (Block.actor && Block.actor->userData)
    {
        OutHit.HitActor = static_cast<AActor*>(Block.actor->userData);
    }
    return true;
}

// ============================================================
// SphereSweep (Start + End 시그니처)
// ============================================================

bool FPhysXPhysicsScene::SphereSweep(const FVector& Start, const FVector& End, float Radius,
    FHitResult& OutHit, ECollisionChannel TraceChannel, const AActor* IgnoreActor) const
{
    if (!Scene) return false;

    FVector Move = End - Start;
    float MoveDist2 = Move.X*Move.X + Move.Y*Move.Y + Move.Z*Move.Z;
    if (MoveDist2 < 1e-12f) return false;
    const float MoveDist = sqrtf(MoveDist2);
    const FVector Dir(Move.X/MoveDist, Move.Y/MoveDist, Move.Z/MoveDist);

    struct FChannelSweepFilter : PxQueryFilterCallback
    {
        const AActor* IgnoreActor = nullptr;
        PxU32 TraceBit = 0;
        FChannelSweepFilter(const AActor* A, ECollisionChannel Ch)
            : IgnoreActor(A), TraceBit(1u << static_cast<PxU32>(Ch)) {}
        PxQueryHitType::Enum preFilter(const PxFilterData&, const PxShape* Shape,
            const PxRigidActor* Actor, PxHitFlags&) override
        {
            if (IgnoreActor && Actor && Actor->userData == IgnoreActor) return PxQueryHitType::eNONE;
            if (Shape && (Shape->getQueryFilterData().word1 & TraceBit) == 0) return PxQueryHitType::eNONE;
            return PxQueryHitType::eBLOCK;
        }
        PxQueryHitType::Enum postFilter(const PxFilterData&, const PxQueryHit&) override
        { return PxQueryHitType::eBLOCK; }
    };

    PxSphereGeometry SphereGeom(Radius);
    PxTransform StartPose(ToPxVec3(Start));
    PxSweepBuffer SweepHit;
    PxQueryFilterData FilterData;
    FilterData.flags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER;
    FChannelSweepFilter FilterCallback(IgnoreActor, TraceChannel);

    bool bStatus = Scene->sweep(SphereGeom, StartPose, ToPxVec3(Dir), MoveDist,
        SweepHit, PxHitFlag::eDEFAULT | PxHitFlag::ePOSITION | PxHitFlag::eNORMAL,
        FilterData, &FilterCallback);
    if (!bStatus || !SweepHit.hasBlock) return false;

    const PxSweepHit& Block = SweepHit.block;
    const FVector ImpactNormal = ToFVector(Block.normal);
    OutHit.bHit             = true;
    OutHit.Distance         = Block.distance;
    OutHit.WorldHitLocation = ToFVector(Block.position);
    OutHit.ShapeLocation    = OutHit.WorldHitLocation + ImpactNormal * Radius;
    OutHit.ImpactNormal     = ImpactNormal;
    OutHit.WorldNormal      = ImpactNormal;

    if (Block.shape && Block.shape->userData)
    {
        OutHit.HitComponent = static_cast<UPrimitiveComponent*>(Block.shape->userData);
        OutHit.HitActor     = OutHit.HitComponent->GetOwner();
    }
    else if (Block.actor && Block.actor->userData)
    {
        OutHit.HitActor = static_cast<AActor*>(Block.actor->userData);
    }
    return true;
}
