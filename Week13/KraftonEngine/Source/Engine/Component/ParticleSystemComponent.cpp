#include "ParticleSystemComponent.h"
#include "Particles/Runtime/ParticleEmitterInstance.h"
#include "Particles/Rendering/ParticleRenderData.h"

void UParticleSystemComponent::InitializeComponent()
{
	ClearEmitterInstances();

	EmitterDelay = 0;
	DeltaTimeTick = 0;
	CustomTimeDilation = 1;

	CurrentLODLevelIndex = 0;
	TotalActiveParticles = 0;

	EmitterMaterials.clear();
	CollisionEvents.clear();
	EmitterRenderData.clear();
}

void UParticleSystemComponent::EndPlay()
{
	DeactivateSystem();
	ClearEmitterInstances();

	UPrimitiveComponent::EndPlay();
}

void UParticleSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction)
{
	UPrimitiveComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Template)
		return;

	if (!IsActive())
	{
		if (!IsActive() && bAutoActivate)
		{
			ActivateSystem();
		}
		else
		{
			return;
		}
	}

	bool bRequiresReset = bResetTriggered;
	bResetTriggered = false;

	if (bRequiresReset){
		ResetSystem();
	}

	DeltaTimeTick = DeltaTime * CustomTimeDilation;
	//AccumLODDistanceCheckTime += DeltaTimeTick;

	//LOD처리
	//if (bCalculateLODLevel == true && AccumLODDistanceCheckTime > Template->LODDistanceCheckTime)
	//{
	//	AccumLODDistanceCheckTime = 0;
	//	FVector EffectLocation = GetWorldLocation();
	//	int32 LODLevel = CalculateLODLevel(EffectLocation);
	//	SetLODLevel(LODLevel);
	//}

	//이벤트 버퍼 정리
	CollisionEvents.clear();
	TotalActiveParticles = 0;

	// 인스턴스 tick
	for (auto instance : EmitterInstances) {
		if (!instance) continue;
		instance->Tick(DeltaTimeTick);
		TotalActiveParticles += instance->ActiveParticles;
	}

	// RenderData 수집
	// BuildRenderData();
	MarkRenderStateDirty();
}

void UParticleSystemComponent::SetTemplate(UParticleSystem* InTemplate)
{
	if (Template == InTemplate) {
		return;
	}

	DeactivateSystem();
	ClearEmitterInstances();

	Template = InTemplate;
	if (Template)
	{
		CreateEmitterInstances();
	}
}

void UParticleSystemComponent::SetLODLevel(int32 InLODLevel)
{
	if (!Template)
		return;

	const int32 MaxLOD = Template->LODDistances.size() - 1;
	if (MaxLOD < 0)
		return;

	const int32 NewLODLevel = FMath::Clamp(InLODLevel, 0, MaxLOD);

	if (CurrentLODLevelIndex == NewLODLevel)
		return;

	CurrentLODLevelIndex = NewLODLevel;

	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			//Instance->SetCurrentLODIndex(CurrentLODLevel, true);
		}
	}

	MarkRenderStateDirty();
}

void UParticleSystemComponent::CreateEmitterInstances()
{
	if (!Template) {
		return;
	}

	ClearEmitterInstances();

	const TArray<UParticleEmitter*> TemplateEmitters = Template->GetEmitters();
	for (auto Emitter : TemplateEmitters) {
		FParticleEmitterInstance* Instance = new FParticleEmitterInstance();
		Instance->Init(this, Emitter);

		EmitterInstances.push_back(Instance);
	}
}

void UParticleSystemComponent::ActivateSystem()
{	
	if (IsActive() || !Template) {
		return;
	}

	bIsActive = true;
	SetComponentTickEnabled(true);

	//LOD처리
	//FVector EffectLocation = GetWorldLocation();
	//int32 LODLevel = CalculateLODLevel(EffectLocation);
	//SetLODLevel(LODLevel);
}

void UParticleSystemComponent::DeactivateSystem()
{
	bIsActive = false;
	SetComponentTickEnabled(false);

	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			Instance->KillAllParticles();
		}
	}

	TotalActiveParticles = 0;
}

void UParticleSystemComponent::ResetSystem()
{
	for (FParticleEmitterInstance* instance : EmitterInstances) {
		instance->Reset();
	}
}

void UParticleSystemComponent::ClearEmitterInstances()
{
	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (Instance)
		{
			Instance->KillAllParticles();
			delete Instance;
		}
	}

	EmitterInstances.clear();
	TotalActiveParticles = 0;
}

void UParticleSystemComponent::SendRenderDynamicData()
{
	if (!SceneProxy)
		return;

	FDynamicEmitterReplayDataBase* NewDynamicData = new FDynamicEmitterReplayDataBase();

	for (FParticleEmitterInstance* Instance : EmitterInstances)
	{
		if (!Instance)
			continue;

		Instance->CreateDynamicEmitterData(*NewDynamicData);
	}
}
