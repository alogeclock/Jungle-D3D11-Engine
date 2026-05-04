#include "ImposterGizmoActorBase.h"
#include "GameFramework/World.h"
#include "Object/Object.h"

#include <algorithm>
#include <random>

DEFINE_CLASS(AImposterGizmoActorBase, AGimmickActorBase)

namespace {
	std::mt19937& RandomEngine()
	{
		static std::mt19937 Engine(std::random_device{}());
		return Engine;
	}
}

void AImposterGizmoActorBase::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);
	if (!Target) return;
	if (!HasAliveTarget())
	{
		Release();
		return;
	}

	if (ElapsedDelay < ActivationDelay) ElapsedDelay += DeltaTime;
	if (ElapsedDelay >= ActivationDelay) Transform(DeltaTime);
}

void AImposterGizmoActorBase::Capture(AActor* InActor) {
	if (!InActor) return;
	if (!IsAliveObject(InActor)) return;
	Target = InActor;
	ElapsedDelay = 0.0f;
	Elapsed = 0.0f;
	bTransforming = false;

	if (!PreviewGizmo)
	{
		PreviewGizmo = AddComponent<UGizmoComponent>();
	}
}

bool AImposterGizmoActorBase::HasAliveTarget() const
{
	return Target && IsAliveObject(Target);
}

uint8 AImposterGizmoActorBase::SetOffsetAxis() {
	std::uniform_int_distribution<int> Distribution(0, 2);
	OffsetAxis = Distribution(RandomEngine());
	return OffsetAxis;
}

FLuaActorProxy AImposterGizmoActorBase::GetCapturedActorProxy() const {
	FLuaActorProxy Proxy;
	Proxy.Actor = HasAliveTarget() ? Target : nullptr;
	return Proxy;
}

AActor* AImposterGizmoActorBase::GetCapturedActor() const
{
	return HasAliveTarget() ? Target : nullptr;
}

void AImposterGizmoActorBase::Release() {
	if (PreviewGizmo && IsAliveObject(PreviewGizmo))
	{
		PreviewGizmo->SetSelectedAxis(-1);
		PreviewGizmo->Deactivate();
	}
	Target = nullptr;
	if (UWorld* World = GetWorld())
	{
		World->DestroyActor(this);
	}
}
