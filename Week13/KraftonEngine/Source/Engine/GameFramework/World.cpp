#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Engine/Component/CameraComponent.h"
#include "Render/Types/LODContext.h"
#include "Physics/Backends/NativePhysicsScene.h"
#include "Physics/Backends/PhysXPhysicsScene.h"
#include "Core/ProjectSettings.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerCameraManager.h"
#include "Math/MathUtils.h"
#include "Object/UClass.h"
#include "Profiling/Stats.h"
#include "Profiling/Timer.h"
#include "Runtime/Engine.h"
#include <algorithm>

namespace
{
	// 월드 내의 모든 PrimitiveComponent에 콜백을 적용하는 유틸리티 함수
	template<typename Func>
	void ForEachPrimitive(UWorld* World, Func&& Callback)
	{
		if (!World) return;

		for (AActor* Actor : World->GetActors())
		{
			if (!Actor) continue;

			for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
			{
				if (Primitive)
				{
					Callback(Primitive);
				}
			}
		}
	}

	// 월드의 모든 PrimitiveComponent 중 SimulatePhysics가 활성화된 컴포넌트에만 콜백을 적용하는 유틸리티 함수
	template<typename Func>
    void ForEachSimulatingPrimitive(UWorld* World, Func&& Callback)
    {
        ForEachPrimitive(World, [&Callback](UPrimitiveComponent* Primitive)
        {
            if (Primitive->GetSimulatePhysics())
            {
                Callback(Primitive);
            }
        });
    }
}

UWorld::~UWorld()
{
	if (PersistentLevel && !PersistentLevel->GetActors().empty())
	{
		EndPlay();
	}
}

UObject* UWorld::Duplicate(UObject* NewOuter) const
{
	// UE의 CreatePIEWorldByDuplication 대응 (간소화 버전).
	// 새 UWorld를 만들고, 소스의 Actor들을 하나씩 복제해 NewWorld를 Outer로 삼아 등록한다.
	// AActor::Duplicate 내부에서 Dup->GetTypedOuter<UWorld>() 경유 AddActor가 호출되므로
	// 여기서는 World 단위 상태만 챙기면 된다.
	UWorld* NewWorld = GUObjectArray.CreateObject<UWorld>();
	if (!NewWorld)
	{
		return nullptr;
	}
	NewWorld->SetOuter(NewOuter);
	NewWorld->WorldSettings = WorldSettings;  // 씬 단위 설정 (GameMode 등) 복제
	NewWorld->InitWorld(); // Partition/VisibleSet 초기화 — 이거 없으면 복제 액터가 렌더링되지 않음

	for (AActor* Src : GetActors())
	{
		if (!Src) continue;
		Src->Duplicate(NewWorld);
	}

	NewWorld->PostDuplicate();
	return NewWorld;
}

UWorld* UWorld::DuplicateAs(EWorldType InWorldType) const
{
	UWorld* NewWorld = GUObjectArray.CreateObject<UWorld>();
	if (!NewWorld) return nullptr;

	NewWorld->SetWorldType(InWorldType);
	NewWorld->WorldSettings = WorldSettings;  // 씬 단위 설정 복제
	NewWorld->InitWorld();

	for (AActor* Src : GetActors())
	{
		if (!Src) continue;
		Src->Duplicate(NewWorld);
	}

	NewWorld->PostDuplicate();
	return NewWorld;
}

void UWorld::DestroyActor(AActor* Actor)
{
	if (!Actor) return;

	Actor->EndPlay();
	PersistentLevel->RemoveActor(Actor);

	MarkWorldPrimitivePickingBVHDirty();
	Partition.RemoveActor(Actor);

	// 즉시 delete. octree / partition 측은 IsValid 가드로 stale 포인터 방어.
	// PhysX onContact / TickManager 순회 도중에 self-destroy 하는 경로 (police HandleHit,
	// meteor Tick) 는 호출자 측에서 stack 위쪽 코드가 더 이상 this 를 만지지 않도록
	// 패턴화해뒀음 (bAlreadyCaught 가드, ElapsedTime=Lifetime 후 다음 Tick).
	GUObjectArray.DestroyObject(Actor);
}

AActor* UWorld::SpawnActorByClass(UClass* Class)
{
	if (!Class) return nullptr;

	UObject* Created = FObjectFactory::Get().Create(Class->GetName(), PersistentLevel);
	AActor* Actor = Cast<AActor>(Created);
	if (!Actor) return nullptr;

	AddActor(Actor);
	return Actor;
}

AGameStateBase* UWorld::GetGameState() const
{
	return GameMode ? GameMode->GetGameState() : nullptr;
}

APlayerController* UWorld::GetFirstPlayerController() const
{
	return GameMode ? GameMode->GetPlayerController() : nullptr;
}

// PC 의 PlayerCameraManager 우선, fallback 으로 IPOVProvider pull.
// PIE/Game 경로는 매니저가 매 프레임 채우는 CameraCachePOV (shake/blend 적용된 최종값) 사용.
bool UWorld::GetActivePOV(FMinimalViewInfo& OutPOV) const
{
	if (APlayerController* PC = GetFirstPlayerController())
	{
		if (APlayerCameraManager* CM = PC->GetPlayerCameraManager())
		{
			if (CM->GetCameraCachePOV(OutPOV))
			{
				return true;
			}
		}
	}
	if (EditorPOVProvider)
	{
		return EditorPOVProvider->GetCameraView(OutPOV);
	}
	return false;
}


void UWorld::AddActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	PersistentLevel->AddActor(Actor);

	InsertActorToOctree(Actor);
	MarkWorldPrimitivePickingBVHDirty();

	// PIE 중 Duplicate(Ctrl+D)나 SpawnActor로 들어온 액터에도 BeginPlay를 보장.
	if (bHasBegunPlay && !Actor->HasActorBegunPlay())
	{
		Actor->BeginPlay();
	}
}

void UWorld::MarkWorldPrimitivePickingBVHDirty()
{
	if (DeferredPickingBVHUpdateDepth > 0)
	{
		bDeferredPickingBVHDirty = true;
		return;
	}

	WorldPrimitivePickingBVH.MarkDirty();
}

void UWorld::BuildWorldPrimitivePickingBVHNow() const
{
	WorldPrimitivePickingBVH.BuildNow(GetActors());
}

void UWorld::BeginDeferredPickingBVHUpdate()
{
	++DeferredPickingBVHUpdateDepth;
}

void UWorld::EndDeferredPickingBVHUpdate()
{
	if (DeferredPickingBVHUpdateDepth <= 0)
	{
		return;
	}

	--DeferredPickingBVHUpdateDepth;
	if (DeferredPickingBVHUpdateDepth == 0 && bDeferredPickingBVHDirty)
	{
		bDeferredPickingBVHDirty = false;
		BuildWorldPrimitivePickingBVHNow();
	}
}

void UWorld::WarmupPickingData() const
{
	for (AActor* Actor : GetActors())
	{
		if (!Actor || !Actor->IsVisible())
		{
			continue;
		}

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			if (!Primitive || !Primitive->IsVisible() || !Primitive->IsA<UStaticMeshComponent>())
			{
				continue;
			}

			UStaticMeshComponent* StaticMeshComponent = static_cast<UStaticMeshComponent*>(Primitive);
			if (UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh())
			{
				StaticMesh->EnsureMeshTrianglePickingBVHBuilt();
			}
		}
	}

	BuildWorldPrimitivePickingBVHNow();
}

bool UWorld::RaycastPrimitives(const FRay& Ray, FHitResult& OutHitResult, AActor*& OutActor) const
{
	//혹시라도 BVH 트리가 업데이트 되지 않았다면 업데이트
	WorldPrimitivePickingBVH.EnsureBuilt(GetActors());
	return WorldPrimitivePickingBVH.Raycast(Ray, OutHitResult, OutActor);
}

bool UWorld::PhysicsRaycast(const FVector& Start, const FVector& Dir, float MaxDist, FHitResult& OutHit,
	ECollisionChannel TraceChannel, const AActor* IgnoreActor) const
{
	if (MaxDist <= 0.0f || Dir.IsNearlyZero())
	{
		return false;
	}

	FVector NormalizedDir = Dir;
	NormalizedDir.Normalize();

	FHitResult BestHit;
	BestHit.Distance = MaxDist;
	bool bFound = false;

	if (PhysicsScene)
	{
		FHitResult PhysicsHit;
		const FVector RayEnd = Start + NormalizedDir * MaxDist;
		if (PhysicsScene->Raycast(Start, RayEnd, PhysicsHit, TraceChannel, IgnoreActor)
			&& PhysicsHit.Distance >= 0.0f
			&& PhysicsHit.Distance <= MaxDist)
		{
			BestHit = PhysicsHit;
			bFound = true;
		}
	}

	// PhysX currently registers simple shape components only. StaticMeshComponent floors/stairs
	// still need query raycasts for gameplay helpers such as foot IK.
	FRay Ray;
	Ray.Origin = Start;
	Ray.Direction = NormalizedDir;

	for (AActor* Actor : GetActors())
	{
		if (!Actor || Actor == IgnoreActor)
		{
			continue;
		}

		for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
		{
			if (!Primitive
				|| !Primitive->IsQueryCollisionEnabled()
				|| Primitive->GetCollisionResponseToChannel(TraceChannel) != ECollisionResponse::Block)
			{
				continue;
			}

			FHitResult CandidateHit;
			if (!Primitive->LineTraceComponent(Ray, CandidateHit) || !CandidateHit.bHit)
			{
				continue;
			}

			if (CandidateHit.Distance < 0.0f || CandidateHit.Distance > BestHit.Distance)
			{
				continue;
			}

			CandidateHit.HitComponent = Primitive;
			CandidateHit.HitActor = Actor;
			CandidateHit.WorldHitLocation = Start + NormalizedDir * CandidateHit.Distance;
			BestHit = CandidateHit;
			bFound = true;
		}
	}

	if (!bFound)
	{
		return false;
	}

	OutHit = BestHit;
	return true;
}


void UWorld::InsertActorToOctree(AActor* Actor)
{
	Partition.InsertActor(Actor);
}

void UWorld::RemoveActorToOctree(AActor* Actor)
{
	Partition.RemoveActor(Actor);
}

void UWorld::UpdateActorInOctree(AActor* Actor)
{
	Partition.UpdateActor(Actor);
}

FLODUpdateContext UWorld::PrepareLODContext()
{
	// 잔여 정리: POV currency 사용. 카메라 인스턴스 비교는 제거 — 위치/회전 변화로만 swap 감지.
	// 거의 같은 위치로 카메라가 swap 되면 한 프레임 stale LOD 가능하지만 다음 프레임 회복.
	FMinimalViewInfo POV;
	if (!GetActivePOV(POV)) return {};

	const FVector CameraPos = POV.Location;
	const FVector CameraForward = POV.Rotation.GetForwardVector();

	const uint32 LODUpdateFrame = VisibleProxyBuildFrame++;
	const uint32 LODUpdateSlice = LODUpdateFrame & (LOD_UPDATE_SLICE_COUNT - 1);
	const bool bShouldStaggerLOD = Scene.GetProxyCount() >= LOD_STAGGER_MIN_VISIBLE;

	const bool bForceFullLODRefresh =
		!bShouldStaggerLOD
		|| !bHasLastFullLODUpdateCameraPos
		|| FVector::DistSquared(CameraPos, LastFullLODUpdateCameraPos) >= LOD_FULL_UPDATE_CAMERA_MOVE_SQ
		|| CameraForward.Dot(LastFullLODUpdateCameraForward) < LOD_FULL_UPDATE_CAMERA_ROTATION_DOT;

	if (bForceFullLODRefresh)
	{
		LastFullLODUpdateCameraPos = CameraPos;
		LastFullLODUpdateCameraForward = CameraForward;
		bHasLastFullLODUpdateCameraPos = true;
	}

	FLODUpdateContext Ctx;
	Ctx.CameraPos = CameraPos;
	Ctx.LODUpdateFrame = LODUpdateFrame;
	Ctx.LODUpdateSlice = LODUpdateSlice;
	Ctx.bForceFullRefresh = bForceFullLODRefresh;
	Ctx.bValid = true;
	return Ctx;
}

void UWorld::InitWorld()
{
	Partition.Reset(FBoundingBox());
	PersistentLevel = GUObjectArray.CreateObject<ULevel>(this);
	PersistentLevel->SetWorld(this);
	PhysicsTimeAccumulator = 0.0f;
	PhysicsInterpolationAlpha = 0.0f;

	// E.2/3: CameraManager spawn 은 PC 의 BeginPlay 가 담당. World 는 보유하지 않음.

	// 물리 시스템 초기화 — ProjectSettings 백엔드 선택
	if (FProjectSettings::Get().Physics.Backend == EPhysicsBackend::PhysX)
		PhysicsScene = std::make_unique<FPhysXPhysicsScene>();
	else
		PhysicsScene = std::make_unique<FNativePhysicsScene>();
	PhysicsScene->InitializeScene(this);
}

void UWorld::BeginPlay()
{
	bHasBegunPlay = true;

	// GameMode spawn — Editor 월드에서는 생성하지 않는다.
	// Level::BeginPlay 이전에 spawn하면 그 루프에서 GameMode/GameState도 BeginPlay된다.
	if (WorldType != EWorldType::Editor && GameModeClass)
	{
		AActor* Spawned = SpawnActorByClass(GameModeClass);
		GameMode = Cast<AGameModeBase>(Spawned);
	}

	if (PersistentLevel)
	{
		PersistentLevel->BeginPlay();
	}

	// 모든 액터 BeginPlay 완료 후 매치 시작 — 페이즈 변경 브로드캐스트가
	// 이때 모든 리스너에게 안전하게 도달한다.
	if (GameMode)
	{
		GameMode->StartMatch();
	}

	// E.2/3: AutoPossessDefaultCamera 는 PC 의 BeginPlay 가 처리.
}

// PhysX 연산을 Fixed Timestep으로 수행하면서 물리 연산과 렌더링 프레임이 분리된다. 
// 이에 따라 렌더링 프레임의 보간을 위한 Physics Scene Snapshot이 필요하다.
void UWorld::Tick(float DeltaTime, ELevelTick TickType)
{
    // Tick 시작 시점의 Primitive Transform을 보간 이전 상태로 초기화
	RestorePhysicsInterpolation();

	{
		SCOPE_STAT_CAT("FlushPrimitive", "1_WorldTick");
		Partition.FlushPrimitive();
	}

	Scene.GetDebugDrawQueue().Tick(DeltaTime);

	// bPaused 동안 PhysicsScene + TickManager skip — GameMode 타이머, Lua Tick, 차량 이동, PhysX 시뮬레이션 모두 정지. 
	// Render / UI / Input poll 은 호출자 (UEngine::Tick)가 따로 돌리므로 영향 X → 메뉴/인트로 위에서 화면 보이고 클릭 가능.
	if (bPaused)
	{
		PhysicsTimeAccumulator = 0.0f;
		PhysicsInterpolationAlpha = 0.0f;
		TickPlayerCamera();
		return;
	}
	
	// ─────────────────────── Begin Frame: 본격적인 프레임 업데이트 시작 ───────────────────────
	TickManager.BeginFrame(this, TickType);

	TickManager.TickGroup(TG_PrePhysics, DeltaTime, TickType); // Pre-Physics

	bool bDispatchedDuringPhysics = false;
	if (bHasBegunPlay && PhysicsScene)
	{
		SCOPE_STAT_CAT("PhysicsScene", "1_WorldTick");
		bDispatchedDuringPhysics = TickPhysics(DeltaTime, TickType);
	}
	else
	{
		ResetPhysicsInterpolation();
		PhysicsTimeAccumulator = 0.0f;
		PhysicsInterpolationAlpha = 0.0f;
	}

	if (!bDispatchedDuringPhysics)
	{
		TickManager.TickGroup(TG_DuringPhysics, DeltaTime, TickType);
	}

	TickManager.TickGroup(TG_PostPhysics, DeltaTime, TickType); // Post-Physics

	TickManager.TickGroup(TG_PostUpdateWork, DeltaTime, TickType);

	// 물리 렌더링 보간 — Fixed Timestep 설정 사용 시, 렌더링 프레임과 물리 틱 사이의 남은 시간을 바탕으로 보간을 적용한다.
	if (bHasBegunPlay && PhysicsScene && FProjectSettings::Get().Physics.bUseFixedTimestep)
	{
		ApplyPhysicsInterpolation(PhysicsInterpolationAlpha);
	}

	// 카메라는 물리/액터 Tick 이후 갱신 — 차량 1인칭처럼 physics body 에 붙은 카메라가
	// 같은 프레임의 최신 transform 으로 POV cache 를 채운다.
	TickPlayerCamera();
}

bool UWorld::TickPhysics(float DeltaTime, ELevelTick TickType)
{
	if (FProjectSettings::Get().Physics.bUseFixedTimestep)
	{
		return TickFixedPhysics(DeltaTime, TickType);
	}
	return TickVariablePhysics(DeltaTime, TickType);
}

// 가변 프레임 방식으로 물리 시뮬레이션을 수행한다.
bool UWorld::TickVariablePhysics(float DeltaTime, ELevelTick TickType)
{
	ResetPhysicsInterpolation();
	PhysicsTimeAccumulator = 0.0f;
	PhysicsInterpolationAlpha = 0.0f;

	// 현재 프레임의 DeltaTime만큼 물리 시뮬레이션을 전진시킨다.
	FPhysicsStepInfo StepInfo;
	StepInfo.DeltaTime = DeltaTime;
	PhysicsScene->Simulate(StepInfo);

	TickManager.TickGroup(TG_DuringPhysics, DeltaTime, TickType);

	PhysicsScene->FetchResults(true);

	return true;
}

// Fixed Timestep 방식으로 물리 시뮬레이션을 수행한다. DeltaTime이 고정된 간격으로 나누어지며, 남은 시간은 보간에 사용된다.
bool UWorld::TickFixedPhysics(float DeltaTime, ELevelTick TickType)
{
	const auto& PhysicsOption = FProjectSettings::Get().Physics;

	// 설정된 고정 물리 틱 시간을 가져오고, 최소/최대치를 제한한다.
	const float FixedDeltaTime = Clamp(PhysicsOption.FixedDeltaTime, 1.0f / 240.0f, 1.0f / 15.0f);
	const int32 MaxSubsteps = (std::max)(1, (std::min)(PhysicsOption.MaxSubsteps, 16));
	const float ClampedFrameDelta = Clamp(DeltaTime, 0.0f, FixedDeltaTime * static_cast<float>(MaxSubsteps));

	PhysicsTimeAccumulator += ClampedFrameDelta;

	bool bDispatchedDuringPhysics = false;
	int32 SubstepIndex = 0;

	// 누적된 시간이 고정 틱 시간보다 큰 동안 물리 스텝을 쪼개며 연산한다.
	while (PhysicsTimeAccumulator + 1.0e-6f >= FixedDeltaTime && SubstepIndex < MaxSubsteps)
	{
		CapturePrePhysicsSnapshot();

		FPhysicsStepInfo StepInfo;
		StepInfo.DeltaTime = FixedDeltaTime;
		StepInfo.SubstepCount = SubstepIndex + 1;
		PhysicsScene->Simulate(StepInfo);

		if (!bDispatchedDuringPhysics)
		{
			TickManager.TickGroup(TG_DuringPhysics, DeltaTime, TickType);
			bDispatchedDuringPhysics = true;
		}

		PhysicsScene->FetchResults(true);
		CapturePostPhysicsSnapshot();

		PhysicsTimeAccumulator -= FixedDeltaTime;
		++SubstepIndex;
	}

	if (PhysicsTimeAccumulator < 0.0f)
	{
		PhysicsTimeAccumulator = 0.0f;
	}

	PhysicsInterpolationAlpha = FixedDeltaTime > 0.0f ? Clamp(PhysicsTimeAccumulator / FixedDeltaTime, 0.0f, 1.0f) : 0.0f;

	return bDispatchedDuringPhysics;
}

// 물리 시뮬레이션 스텝 진입 직전의 위치/회전 값을 보간 시작점으로 스냅샷을 찍는다.
void UWorld::CapturePrePhysicsSnapshot()
{
	if (!FProjectSettings::Get().Physics.bEnableRenderInterpolation) return;

	ForEachSimulatingPrimitive(this, [](UPrimitiveComponent* Primitive)
	{
		Primitive->CapturePrePhysicsSnapshot();
	});
}

// 물리 시뮬레이션 스텝 직후의 위치/회전 값을 보간 목표점으로 스냅샷을 찍는다.
void UWorld::CapturePostPhysicsSnapshot()
{
	if (!FProjectSettings::Get().Physics.bEnableRenderInterpolation) return;

	ForEachSimulatingPrimitive(this, [](UPrimitiveComponent* Primitive)
	{
		Primitive->CapturePostPhysicsSnapshot();
	});
}

// 계산된 Alpha 값을 바탕으로 월드의 프리미티브의 모든 이전/현재 트랜스폼을 보간한다.
void UWorld::ApplyPhysicsInterpolation(float Alpha)
{
	if (!FProjectSettings::Get().Physics.bEnableRenderInterpolation)
	{
		return;
	}

	ForEachSimulatingPrimitive(this, [Alpha](UPrimitiveComponent* Primitive)
	{
		Primitive->ApplyPhysicsInterpolation(Alpha);
	});
}

// 렌더링을 위해 보간되었던 트랜스폼을 실제 물리 시뮬레이션 결과로 되돌린다.
void UWorld::RestorePhysicsInterpolation()
{
	ForEachPrimitive(this, [](UPrimitiveComponent* Primitive)
	{
		Primitive->RestorePhysicsInterpolation();
	});
}

// 보간과 관련된 모든 트랜스폼 상태를 초기화한다.
void UWorld::ResetPhysicsInterpolation()
{
	ForEachPrimitive(this, [](UPrimitiveComponent* Primitive)
	{
		Primitive->ResetPhysicsInterpolation();
	});
}

void UWorld::TickPlayerCamera() const
{
	APlayerController* PC = GetFirstPlayerController();
	APlayerCameraManager* CM = PC ? PC->GetPlayerCameraManager() : nullptr;
	if (!CM)
	{
		return;
	}

	// Shake / Fade timer / blend 는 Slomo (TimeDilation < 1.0) 영향을 받으면 효과가
	// 늘어붙어 보이므로 raw delta 를 사용한다. paused 중에도 timer 진행은 동일.
	const FTimer* Timer = GEngine ? GEngine->GetTimer() : nullptr;
	const float UnscaledDelta = Timer ? Timer->GetRawDeltaTime() : 0.0f;
	CM->UpdateCamera(UnscaledDelta);
}

void UWorld::EndPlay()
{
	RestorePhysicsInterpolation();
	ResetPhysicsInterpolation();

	if (GameMode)
	{
		GameMode->EndMatch();
	}

	bHasBegunPlay = false;
	TickManager.Reset();

	if (!PersistentLevel)
	{
		return;
	}

	PersistentLevel->EndPlay();

	// 물리 시스템 정리 — 액터/컴포넌트가 아직 살아있는 동안 해제
	if (PhysicsScene)
	{
		PhysicsScene->ReleaseScene();
	}

	// Clear spatial partition while actors/components are still alive.
	// Otherwise Octree teardown can dereference stale primitive pointers during shutdown.
	Partition.Reset(FBoundingBox());

	PersistentLevel->Clear();
	GameMode = nullptr; // 액터 리스트가 비워지면서 dangling 되므로 명시적으로 해제
	MarkWorldPrimitivePickingBVHDirty();

	// PersistentLevel은 CreateObject로 생성되었으므로 DestroyObject로 해제해야 alloc count가 맞음
	GUObjectArray.DestroyObject(PersistentLevel);
	PersistentLevel = nullptr;
}
