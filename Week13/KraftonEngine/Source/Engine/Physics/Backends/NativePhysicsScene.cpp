#include "Physics/Backends/NativePhysicsScene.h"
#include "Collision/CollisionMath.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"

#include <algorithm>

// ============================================================
// Lifecycle
// ============================================================

bool FNativePhysicsScene::InitializeScene(UWorld* InWorld, EPhysicsSceneType SceneType)
{
    World = InWorld;
    SetSceneType(SceneType);
    return true;
}

void FNativePhysicsScene::ReleaseScene()
{
    for (auto& Pair : BodyInstances)
    {
        delete Pair.second;
    }
    BodyInstances.clear();
    RegisteredComponents.clear();
    BodyStates.clear();
    PreviousOverlaps.clear();
    CurrentOverlaps.clear();
    PreviousBlockPairs.clear();
    CurrentBlockPairs.clear();
    ClearRegisteredInstances();
    World = nullptr;
}

// ============================================================
// Body 관리
// ============================================================

FPhysicsBodyInstance* FNativePhysicsScene::CreateBody(UPrimitiveComponent* Comp, const FPhysicsBodyDesc& BodyDesc)
{
    if (!Comp) return nullptr;

    // 이미 등록된 경우 기존 인스턴스 반환
    auto ExistingIt = BodyInstances.find(Comp);
    if (ExistingIt != BodyInstances.end())
        return ExistingIt->second;

    // 내부 컴포넌트 리스트에 추가
    RegisteredComponents.push_back(Comp);
    FBodyState& State = BodyStates[Comp];
    State.Mass = Comp->GetMass() > 0.0f ? Comp->GetMass() : 1.0f;
    State.CenterOfMassLocal = Comp->GetCenterOfMass();

    // FPhysicsBodyInstance 생성
    FPhysicsBodyInstance* Instance = new FPhysicsBodyInstance();
    Instance->SetOwnerComponent(Comp);
    Instance->SetBodyDesc(BodyDesc);

    FPhysicsActorHandle Handle;
    Handle.NativeActor = static_cast<void*>(Comp); // Native: 컴포넌트 포인터를 핸들로 사용
    Instance->SetActorHandle(Handle);
    Instance->SetActorState(EPhysicsActorState::PAS_Added);

    BodyInstances[Comp] = Instance;
    RegisterBodyInstance(Instance);
    GetMutableRuntimeStats().BodyCount = static_cast<int32>(RegisteredComponents.size());
    return Instance;
}

void FNativePhysicsScene::DestroyBody(FPhysicsBodyInstance* BodyInstance)
{
    if (!BodyInstance) return;
    UPrimitiveComponent* Comp = BodyInstance->GetOwnerComponent();
    if (!Comp) return;

    auto It = std::find(RegisteredComponents.begin(), RegisteredComponents.end(), Comp);
    if (It == RegisteredComponents.end()) return;
    RegisteredComponents.erase(It);
    BodyStates.erase(Comp);

    // PreviousOverlaps에서 이 컴포넌트를 포함하는 쌍 제거 + EndOverlap 발화
    auto PairIt = PreviousOverlaps.begin();
    while (PairIt != PreviousOverlaps.end())
    {
        if (PairIt->A == Comp || PairIt->B == Comp)
        {
            UPrimitiveComponent* Other = (PairIt->A == Comp) ? PairIt->B : PairIt->A;
            if (Other->GetGenerateOverlapEvents())
            {
                AActor* CompOwner = Comp->GetOwner();
                Other->NotifyComponentEndOverlap(Other, CompOwner, Comp, 0);
            }
            PairIt = PreviousOverlaps.erase(PairIt);
        }
        else
        {
            ++PairIt;
        }
    }

    // CurrentOverlaps / BlockPairs 정리
    auto EraseFromSet = [Comp](std::unordered_set<FOverlapPair>& Set) {
        auto It = Set.begin();
        while (It != Set.end())
        {
            if (It->A == Comp || It->B == Comp)
                It = Set.erase(It);
            else
                ++It;
        }
    };
    EraseFromSet(CurrentOverlaps);
    EraseFromSet(PreviousBlockPairs);
    EraseFromSet(CurrentBlockPairs);

    BodyInstances.erase(Comp);
    UnregisterBodyInstance(BodyInstance);

    BodyInstance->SetActorState(EPhysicsActorState::PAS_Destroyed);
    BodyInstance->GetMutableActorHandle().Reset();
    delete BodyInstance;

    GetMutableRuntimeStats().BodyCount = static_cast<int32>(RegisteredComponents.size());
}

FPhysicsConstraintInstance* FNativePhysicsScene::CreateConstraint(
    FPhysicsBodyInstance* /*ParentBody*/,
    FPhysicsBodyInstance* /*ChildBody*/,
    const FPhysicsConstraintDesc& /*ConstraintDesc*/)
{
    return nullptr; // Native 백엔드에서는 미지원
}

void FNativePhysicsScene::DestroyConstraint(FPhysicsConstraintInstance* /*ConstraintInstance*/)
{
    // noop
}

void FNativePhysicsScene::RebuildBody(UPrimitiveComponent* Comp)
{
    auto It = BodyStates.find(Comp);
    if (It == BodyStates.end()) return;
    It->second.Mass = (Comp->GetMass() > 0.0f) ? Comp->GetMass() : 1.0f;
    It->second.CenterOfMassLocal = Comp->GetCenterOfMass();
}

// ============================================================
// 시뮬레이션 (구 Tick)
// ============================================================

void FNativePhysicsScene::Simulate(const FPhysicsStepInfo& StepInfo)
{
    if (!World) return;
    const float DeltaTime = StepInfo.DeltaTime;

    // 힘 적분 + 중력
    for (UPrimitiveComponent* Comp : RegisteredComponents)
    {
        if (!Comp->GetSimulatePhysics()) continue;

        FBodyState& State = BodyStates[Comp];
        float InvMass = (State.Mass > 0.0f) ? (1.0f / State.Mass) : 0.0f;
        State.Velocity = State.Velocity + State.AccumulatedForce * InvMass * DeltaTime;
        State.AngularVelocity = State.AngularVelocity + State.AccumulatedTorque * InvMass * DeltaTime;
        State.Velocity.Z += GravityZ * DeltaTime;

        FVector Pos = Comp->GetWorldLocation();
        Pos = Pos + State.Velocity * DeltaTime;
        Comp->SetWorldLocation(Pos);

        State.AccumulatedForce  = { 0, 0, 0 };
        State.AccumulatedTorque = { 0, 0, 0 };
    }

    CurrentOverlaps.clear();
    CurrentBlockPairs.clear();

    const int32 Count = static_cast<int32>(RegisteredComponents.size());
    for (int32 i = 0; i < Count; ++i)
    {
        for (int32 j = i + 1; j < Count; ++j)
        {
            UPrimitiveComponent* A = RegisteredComponents[i];
            UPrimitiveComponent* B = RegisteredComponents[j];

            if (A->GetOwner() == B->GetOwner()) continue;

            ECollisionResponse Resp = UPrimitiveComponent::GetMinResponse(A, B);
            if (Resp == ECollisionResponse::Ignore) continue;

            FBoundingBox BoundsA = A->GetWorldBoundingBox();
            FBoundingBox BoundsB = B->GetWorldBoundingBox();
            if (!FCollisionMath::AABBvsAABB(BoundsA.Min, BoundsA.Max, BoundsB.Min, BoundsB.Max))
                continue;

            FHitResult Hit;
            if (!FCollisionMath::TestComponentPair(A, B, Hit))
                continue;

            if (Resp == ECollisionResponse::Block)
            {
                CurrentBlockPairs.insert(FOverlapPair{ A, B });

                if (PreviousBlockPairs.find(FOverlapPair{ A, B }) == PreviousBlockPairs.end())
                {
                    FVector NormalImpulse = Hit.ImpactNormal * Hit.PenetrationDepth;

                    FHitResult HitA = Hit;
                    HitA.HitComponent = B;
                    HitA.HitActor = B->GetOwner();
                    A->NotifyComponentHit(A, B->GetOwner(), B, NormalImpulse, HitA);

                    FHitResult HitB = Hit;
                    HitB.HitComponent = A;
                    HitB.HitActor = A->GetOwner();
                    HitB.ImpactNormal = Hit.ImpactNormal * -1.0f;
                    HitB.WorldNormal  = Hit.WorldNormal  * -1.0f;
                    B->NotifyComponentHit(B, A->GetOwner(), A, NormalImpulse * -1.0f, HitB);
                }

                bool bASimulates = A->GetSimulatePhysics();
                bool bBSimulates = B->GetSimulatePhysics();
                FVector PushA;
                if (Hit.HitComponent == B)
                    PushA = Hit.ImpactNormal * -1.0f;
                else
                    PushA = Hit.ImpactNormal;
                FVector Normal = PushA;
                float Depth = Hit.PenetrationDepth;

                constexpr float Slop = 0.01f;
                constexpr float BaumgarteBeta = 0.2f;
                float CorrectionDepth = (std::max)(0.0f, Depth - Slop);

                if (CorrectionDepth > 0.0f)
                {
                    float BiasVelocity = (BaumgarteBeta / DeltaTime) * CorrectionDepth;

                    if (bASimulates && bBSimulates)
                    {
                        FVector Correction = Normal * (BiasVelocity * DeltaTime * 0.5f);
                        A->SetWorldLocation(A->GetWorldLocation() + Correction);
                        B->SetWorldLocation(B->GetWorldLocation() - Correction);
                    }
                    else if (bASimulates)
                    {
                        A->SetWorldLocation(A->GetWorldLocation() + Normal * (BiasVelocity * DeltaTime));
                    }
                    else if (bBSimulates)
                    {
                        B->SetWorldLocation(B->GetWorldLocation() - Normal * (BiasVelocity * DeltaTime));
                    }
                }

                constexpr float Restitution = 0.2f;
                if (bASimulates)
                {
                    FBodyState& StateA = BodyStates[A];
                    float VdotN = StateA.Velocity.X * Normal.X + StateA.Velocity.Y * Normal.Y + StateA.Velocity.Z * Normal.Z;
                    if (VdotN < 0.0f)
                        StateA.Velocity = StateA.Velocity - Normal * (VdotN * (1.0f + Restitution));
                }
                if (bBSimulates)
                {
                    FBodyState& StateB = BodyStates[B];
                    FVector NegNormal = Normal * -1.0f;
                    float VdotN = StateB.Velocity.X * NegNormal.X + StateB.Velocity.Y * NegNormal.Y + StateB.Velocity.Z * NegNormal.Z;
                    if (VdotN < 0.0f)
                        StateB.Velocity = StateB.Velocity - NegNormal * (VdotN * (1.0f + Restitution));
                }
            }
            else if (Resp == ECollisionResponse::Overlap)
            {
                if (A->GetGenerateOverlapEvents() || B->GetGenerateOverlapEvents())
                    CurrentOverlaps.insert(FOverlapPair{ A, B });
            }
        }
    }

    // Begin Overlap
    for (const FOverlapPair& Pair : CurrentOverlaps)
    {
        if (PreviousOverlaps.find(Pair) == PreviousOverlaps.end())
        {
            FHitResult DummyHit;
            if (Pair.A->GetGenerateOverlapEvents())
                Pair.A->NotifyComponentBeginOverlap(Pair.A, Pair.B->GetOwner(), Pair.B, 0, false, DummyHit);
            if (Pair.B->GetGenerateOverlapEvents())
                Pair.B->NotifyComponentBeginOverlap(Pair.B, Pair.A->GetOwner(), Pair.A, 0, false, DummyHit);
        }
    }

    // End Overlap
    for (const FOverlapPair& Pair : PreviousOverlaps)
    {
        if (CurrentOverlaps.find(Pair) == CurrentOverlaps.end())
        {
            if (Pair.A->GetGenerateOverlapEvents())
                Pair.A->NotifyComponentEndOverlap(Pair.A, Pair.B->GetOwner(), Pair.B, 0);
            if (Pair.B->GetGenerateOverlapEvents())
                Pair.B->NotifyComponentEndOverlap(Pair.B, Pair.A->GetOwner(), Pair.A, 0);
        }
    }

    // End Hit
    for (const FOverlapPair& Pair : PreviousBlockPairs)
    {
        if (CurrentBlockPairs.find(Pair) == CurrentBlockPairs.end())
        {
            Pair.A->NotifyComponentEndHit(Pair.A, Pair.B->GetOwner(), Pair.B);
            Pair.B->NotifyComponentEndHit(Pair.B, Pair.A->GetOwner(), Pair.A);
        }
    }

    PreviousOverlaps   = CurrentOverlaps;
    PreviousBlockPairs = CurrentBlockPairs;

    GetMutableRuntimeStats().ContactCount = static_cast<int32>(CurrentBlockPairs.size());
}

void FNativePhysicsScene::FetchResults(bool /*bBlock*/)
{
    // Native 백엔드는 Simulate()에서 동기적으로 모두 처리 — noop
}

// ============================================================
// Force / Torque
// ============================================================

void FNativePhysicsScene::AddForce(UPrimitiveComponent* Comp, const FVector& Force)
{
    auto It = BodyStates.find(Comp);
    if (It == BodyStates.end()) return;
    It->second.AccumulatedForce = It->second.AccumulatedForce + Force;
}

void FNativePhysicsScene::AddForceAtLocation(UPrimitiveComponent* Comp, const FVector& Force, const FVector& WorldLocation)
{
    auto It = BodyStates.find(Comp);
    if (It == BodyStates.end()) return;

    It->second.AccumulatedForce = It->second.AccumulatedForce + Force;

    FVector COMWorld = Comp->GetWorldLocation()
        + Comp->GetWorldMatrix().ToQuat().RotateVector(It->second.CenterOfMassLocal);
    FVector R = WorldLocation - COMWorld;
    FVector Torque;
    Torque.X = R.Y * Force.Z - R.Z * Force.Y;
    Torque.Y = R.Z * Force.X - R.X * Force.Z;
    Torque.Z = R.X * Force.Y - R.Y * Force.X;
    It->second.AccumulatedTorque = It->second.AccumulatedTorque + Torque;
}

void FNativePhysicsScene::AddTorque(UPrimitiveComponent* Comp, const FVector& Torque)
{
    auto It = BodyStates.find(Comp);
    if (It == BodyStates.end()) return;
    It->second.AccumulatedTorque = It->second.AccumulatedTorque + Torque;
}

// ============================================================
// Velocity
// ============================================================

FVector FNativePhysicsScene::GetLinearVelocity(UPrimitiveComponent* Comp) const
{
    auto It = BodyStates.find(Comp);
    return (It != BodyStates.end()) ? It->second.Velocity : FVector(0, 0, 0);
}

void FNativePhysicsScene::SetLinearVelocity(UPrimitiveComponent* Comp, const FVector& Vel)
{
    auto It = BodyStates.find(Comp);
    if (It != BodyStates.end()) It->second.Velocity = Vel;
}

FVector FNativePhysicsScene::GetAngularVelocity(UPrimitiveComponent* Comp) const
{
    auto It = BodyStates.find(Comp);
    return (It != BodyStates.end()) ? It->second.AngularVelocity : FVector(0, 0, 0);
}

void FNativePhysicsScene::SetAngularVelocity(UPrimitiveComponent* Comp, const FVector& Vel)
{
    auto It = BodyStates.find(Comp);
    if (It != BodyStates.end()) It->second.AngularVelocity = Vel;
}

// ============================================================
// Mass
// ============================================================

void FNativePhysicsScene::SetMass(UPrimitiveComponent* Comp, float Mass)
{
    auto It = BodyStates.find(Comp);
    if (It != BodyStates.end()) It->second.Mass = (Mass > 0.0f) ? Mass : 1.0f;
}

float FNativePhysicsScene::GetMass(UPrimitiveComponent* Comp) const
{
    auto It = BodyStates.find(Comp);
    return (It != BodyStates.end()) ? It->second.Mass : 1.0f;
}

void FNativePhysicsScene::SetCenterOfMass(UPrimitiveComponent* Comp, const FVector& LocalOffset)
{
    auto It = BodyStates.find(Comp);
    if (It != BodyStates.end()) It->second.CenterOfMassLocal = LocalOffset;
}

FVector FNativePhysicsScene::GetCenterOfMass(UPrimitiveComponent* Comp) const
{
    auto It = BodyStates.find(Comp);
    return (It != BodyStates.end()) ? It->second.CenterOfMassLocal : FVector(0, 0, 0);
}

// ============================================================
// Raycast — brute-force AABB ray test (Start + End 시그니처)
// ============================================================

bool FNativePhysicsScene::Raycast(const FVector& Start, const FVector& End, FHitResult& OutHit,
    ECollisionChannel TraceChannel, const AActor* IgnoreActor) const
{
    FVector Move = End - Start;
    float MoveDist2 = Move.X * Move.X + Move.Y * Move.Y + Move.Z * Move.Z;
    if (MoveDist2 < 1e-12f) return false;
    const float MaxDist = sqrtf(MoveDist2);

    FVector Dir;
    Dir.X = Move.X / MaxDist;
    Dir.Y = Move.Y / MaxDist;
    Dir.Z = Move.Z / MaxDist;

    FVector InvDir;
    InvDir.X = (Dir.X != 0.0f) ? (1.0f / Dir.X) : 1e30f;
    InvDir.Y = (Dir.Y != 0.0f) ? (1.0f / Dir.Y) : 1e30f;
    InvDir.Z = (Dir.Z != 0.0f) ? (1.0f / Dir.Z) : 1e30f;

    float ClosestDist = MaxDist;
    bool bFound = false;

    for (UPrimitiveComponent* Comp : RegisteredComponents)
    {
        if (IgnoreActor && Comp->GetOwner() == IgnoreActor) continue;
        if (Comp->GetCollisionResponseToChannel(TraceChannel) != ECollisionResponse::Block) continue;

        FBoundingBox Box = Comp->GetWorldBoundingBox();

        float tMin = (Box.Min.X - Start.X) * InvDir.X;
        float tMax = (Box.Max.X - Start.X) * InvDir.X;
        if (tMin > tMax) { float tmp = tMin; tMin = tMax; tMax = tmp; }

        float tyMin = (Box.Min.Y - Start.Y) * InvDir.Y;
        float tyMax = (Box.Max.Y - Start.Y) * InvDir.Y;
        if (tyMin > tyMax) { float tmp = tyMin; tyMin = tyMax; tyMax = tmp; }

        if ((tMin > tyMax) || (tyMin > tMax)) continue;
        if (tyMin > tMin) tMin = tyMin;
        if (tyMax < tMax) tMax = tyMax;

        float tzMin = (Box.Min.Z - Start.Z) * InvDir.Z;
        float tzMax = (Box.Max.Z - Start.Z) * InvDir.Z;
        if (tzMin > tzMax) { float tmp = tzMin; tzMin = tzMax; tzMax = tmp; }

        if ((tMin > tzMax) || (tzMin > tMax)) continue;
        if (tzMin > tMin) tMin = tzMin;

        if (tMin < 0.0f) tMin = 0.0f;
        if (tMin >= ClosestDist) continue;

        ClosestDist = tMin;
        bFound = true;

        OutHit.bHit = true;
        OutHit.Distance = tMin;
        OutHit.HitComponent = Comp;
        OutHit.HitActor = Comp->GetOwner();
        OutHit.WorldHitLocation = Start + Dir * tMin;

        FVector Normal = FVector::ZeroVector;
        if (tMin == tzMin)
            Normal.Z = (Dir.Z > 0.0f) ? -1.0f : 1.0f;
        else if (tMin == tyMin)
            Normal.Y = (Dir.Y > 0.0f) ? -1.0f : 1.0f;
        else
            Normal.X = (Dir.X > 0.0f) ? -1.0f : 1.0f;
        OutHit.ImpactNormal  = Normal;
        OutHit.WorldNormal   = Normal;
        OutHit.ShapeLocation = OutHit.WorldHitLocation;
    }

    return bFound;
}

// ============================================================
// SphereSweepApprox_ExpandedAABB
// ============================================================

bool FNativePhysicsScene::SphereSweepApprox_ExpandedAABB(const FVector& Start, const FVector& End, float Radius,
    FHitResult& OutHit, ECollisionChannel TraceChannel, const AActor* IgnoreActor) const
{
    FVector Move = End - Start;
    float MoveDist = Move.X * Move.X + Move.Y * Move.Y + Move.Z * Move.Z;
    if (MoveDist < 1e-8f) return false;
    MoveDist = sqrtf(MoveDist);

    FVector Dir;
    Dir.X = Move.X / MoveDist; Dir.Y = Move.Y / MoveDist; Dir.Z = Move.Z / MoveDist;

    FVector InvDir;
    InvDir.X = (Dir.X != 0.0f) ? (1.0f / Dir.X) : 1e30f;
    InvDir.Y = (Dir.Y != 0.0f) ? (1.0f / Dir.Y) : 1e30f;
    InvDir.Z = (Dir.Z != 0.0f) ? (1.0f / Dir.Z) : 1e30f;

    float ClosestDist = MoveDist;
    bool bFound = false;

    for (UPrimitiveComponent* Comp : RegisteredComponents)
    {
        if (IgnoreActor && Comp->GetOwner() == IgnoreActor) continue;
        if (Comp->GetCollisionResponseToChannel(TraceChannel) != ECollisionResponse::Block) continue;

        FBoundingBox Box = Comp->GetWorldBoundingBox();

        const float ExMin_X = Box.Min.X - Radius, ExMax_X = Box.Max.X + Radius;
        const float ExMin_Y = Box.Min.Y - Radius, ExMax_Y = Box.Max.Y + Radius;
        const float ExMin_Z = Box.Min.Z - Radius, ExMax_Z = Box.Max.Z + Radius;

        float txMin = (ExMin_X - Start.X) * InvDir.X;
        float txMax = (ExMax_X - Start.X) * InvDir.X;
        if (txMin > txMax) { float t = txMin; txMin = txMax; txMax = t; }

        float tyMin = (ExMin_Y - Start.Y) * InvDir.Y;
        float tyMax = (ExMax_Y - Start.Y) * InvDir.Y;
        if (tyMin > tyMax) { float t = tyMin; tyMin = tyMax; tyMax = t; }

        if (txMin > tyMax || tyMin > txMax) continue;

        float tzMin = (ExMin_Z - Start.Z) * InvDir.Z;
        float tzMax = (ExMax_Z - Start.Z) * InvDir.Z;
        if (tzMin > tzMax) { float t = tzMin; tzMin = tzMax; tzMax = t; }

        int HitAxis = 0;
        float tEnter = txMin;
        if (tyMin > tEnter) { tEnter = tyMin; HitAxis = 1; }
        if (tzMin > tEnter) { tEnter = tzMin; HitAxis = 2; }

        float tExit = txMax < tyMax ? txMax : tyMax;
        if (tzMax < tExit) tExit = tzMax;

        if (tEnter > tExit) continue;
        if (tEnter < 0.0f) tEnter = 0.0f;
        if (tEnter >= ClosestDist) continue;

        FVector Normal = FVector::ZeroVector;
        if      (HitAxis == 0) Normal.X = (Dir.X > 0.0f) ? -1.0f : 1.0f;
        else if (HitAxis == 1) Normal.Y = (Dir.Y > 0.0f) ? -1.0f : 1.0f;
        else                   Normal.Z = (Dir.Z > 0.0f) ? -1.0f : 1.0f;

        ClosestDist = tEnter;
        bFound = true;

        FVector SphereCenter(Start.X + Dir.X * tEnter, Start.Y + Dir.Y * tEnter, Start.Z + Dir.Z * tEnter);

        OutHit.bHit = true;
        OutHit.Distance = tEnter;
        OutHit.HitComponent = Comp;
        OutHit.HitActor = Comp->GetOwner();
        OutHit.ShapeLocation    = SphereCenter;
        OutHit.WorldHitLocation = SphereCenter - Normal * Radius;
        OutHit.ImpactNormal = Normal;
        OutHit.WorldNormal  = Normal;
    }

    return bFound;
}

// ============================================================
// SphereSweep — 3-phase (Face / Edge / Corner)
// ============================================================

bool FNativePhysicsScene::SphereSweep(const FVector& Start, const FVector& End, float Radius,
    FHitResult& OutHit, ECollisionChannel TraceChannel, const AActor* IgnoreActor) const
{
    FVector Move = End - Start;
    float MoveDist2 = Move.X*Move.X + Move.Y*Move.Y + Move.Z*Move.Z;
    if (MoveDist2 < 1e-8f) return false;
    const float MoveDist = sqrtf(MoveDist2);
    const float DX = Move.X / MoveDist, DY = Move.Y / MoveDist, DZ = Move.Z / MoveDist;
    const float R2 = Radius * Radius;

    float ClosestDist = MoveDist;
    bool bFound = false;

    for (UPrimitiveComponent* Comp : RegisteredComponents)
    {
        if (IgnoreActor && Comp->GetOwner() == IgnoreActor) continue;
        if (Comp->GetCollisionResponseToChannel(TraceChannel) != ECollisionResponse::Block) continue;

        FBoundingBox Box = Comp->GetWorldBoundingBox();

        const float S[3]    = { Start.X, Start.Y, Start.Z };
        const float D[3]    = { DX, DY, DZ };
        const float BMin[3] = { Box.Min.X, Box.Min.Y, Box.Min.Z };
        const float BMax[3] = { Box.Max.X, Box.Max.Y, Box.Max.Z };

        float tBest = ClosestDist;
        float NX = 0.f, NY = 0.f, NZ = 0.f;
        bool bHitThis = false;

        auto TryHit = [&](float t, float nx, float ny, float nz)
        {
            if (t >= 0.0f && t < tBest) { tBest = t; NX = nx; NY = ny; NZ = nz; bHitThis = true; }
        };

        // Phase 1: Face
        for (int ax = 0; ax < 3; ++ax)
        {
            const int a1 = (ax+1)%3, a2 = (ax+2)%3;
            if (D[ax] > 1e-8f)
            {
                const float t = (BMin[ax] - Radius - S[ax]) / D[ax];
                if (t >= 0.0f && t < tBest)
                {
                    const float p1 = S[a1] + t*D[a1], p2 = S[a2] + t*D[a2];
                    if (p1 >= BMin[a1] && p1 <= BMax[a1] && p2 >= BMin[a2] && p2 <= BMax[a2])
                    { float n[3]={0,0,0}; n[ax]=-1.f; TryHit(t,n[0],n[1],n[2]); }
                }
            }
            if (D[ax] < -1e-8f)
            {
                const float t = (BMax[ax] + Radius - S[ax]) / D[ax];
                if (t >= 0.0f && t < tBest)
                {
                    const float p1 = S[a1] + t*D[a1], p2 = S[a2] + t*D[a2];
                    if (p1 >= BMin[a1] && p1 <= BMax[a1] && p2 >= BMin[a2] && p2 <= BMax[a2])
                    { float n[3]={0,0,0}; n[ax]=1.f; TryHit(t,n[0],n[1],n[2]); }
                }
            }
        }

        // Phase 2: Edge
        for (int edgeAx = 0; edgeAx < 3; ++edgeAx)
        {
            const int a1 = (edgeAx+1)%3, a2 = (edgeAx+2)%3;
            for (int s1 = 0; s1 < 2; ++s1)
            for (int s2 = 0; s2 < 2; ++s2)
            {
                const float ec1 = s1 ? BMax[a1] : BMin[a1];
                const float ec2 = s2 ? BMax[a2] : BMin[a2];
                const float da1 = S[a1]-ec1, da2 = S[a2]-ec2;
                const float qa = D[a1]*D[a1]+D[a2]*D[a2];
                if (qa < 1e-10f) continue;
                const float qb   = da1*D[a1]+da2*D[a2];
                const float qc   = da1*da1+da2*da2-R2;
                const float disc = qb*qb-qa*qc;
                if (disc < 0.0f) continue;
                const float t = (-qb-sqrtf(disc))/qa;
                if (t < 0.0f || t >= tBest) continue;
                const float hitOnAx = S[edgeAx]+t*D[edgeAx];
                if (hitOnAx < BMin[edgeAx] || hitOnAx > BMax[edgeAx]) continue;
                float ha1 = S[a1]+t*D[a1]-ec1, ha2 = S[a2]+t*D[a2]-ec2;
                float len = sqrtf(ha1*ha1+ha2*ha2);
                if (len < 1e-8f) { ha1=s1?1.f:-1.f; ha2=s2?1.f:-1.f; len=sqrtf(2.f); }
                ha1/=len; ha2/=len;
                float n[3]={0,0,0}; n[a1]=ha1; n[a2]=ha2;
                TryHit(t,n[0],n[1],n[2]);
            }
        }

        // Phase 3: Corner
        for (int cx=0;cx<2;++cx)
        for (int cy=0;cy<2;++cy)
        for (int cz=0;cz<2;++cz)
        {
            const float CX=cx?BMax[0]:BMin[0], CY=cy?BMax[1]:BMin[1], CZ=cz?BMax[2]:BMin[2];
            const float dx=S[0]-CX, dy=S[1]-CY, dz=S[2]-CZ;
            const float b=dx*D[0]+dy*D[1]+dz*D[2];
            const float c=dx*dx+dy*dy+dz*dz-R2;
            const float disc=b*b-c;
            if (disc<0.f) continue;
            const float t=-b-sqrtf(disc);
            if (t<0.f||t>=tBest) continue;
            const float hx=S[0]+t*D[0],hy=S[1]+t*D[1],hz=S[2]+t*D[2];
            if (cx==0?(hx>BMin[0]):(hx<BMax[0])) continue;
            if (cy==0?(hy>BMin[1]):(hy<BMax[1])) continue;
            if (cz==0?(hz>BMin[2]):(hz<BMax[2])) continue;
            float nx=hx-CX,ny=hy-CY,nz=hz-CZ;
            const float nlen=sqrtf(nx*nx+ny*ny+nz*nz);
            if (nlen>1e-8f){nx/=nlen;ny/=nlen;nz/=nlen;}
            else{nx=cx?1.f:-1.f;ny=cy?1.f:-1.f;nz=cz?1.f:-1.f;const float nl=sqrtf(3.f);nx/=nl;ny/=nl;nz/=nl;}
            TryHit(t,nx,ny,nz);
        }

        if (bHitThis)
        {
            ClosestDist = tBest;
            bFound = true;
            const FVector SphereCenter(Start.X+DX*tBest, Start.Y+DY*tBest, Start.Z+DZ*tBest);
            const FVector Normal(NX,NY,NZ);
            OutHit.bHit           = true;
            OutHit.Distance       = tBest;
            OutHit.HitComponent   = Comp;
            OutHit.HitActor       = Comp->GetOwner();
            OutHit.ShapeLocation  = SphereCenter;
            OutHit.WorldHitLocation = SphereCenter - Normal * Radius;
            OutHit.ImpactNormal   = Normal;
            OutHit.WorldNormal    = Normal;
        }
    }

    return bFound;
}
