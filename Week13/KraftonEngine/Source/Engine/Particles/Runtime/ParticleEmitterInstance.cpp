#include "ParticleEmitterInstance.h"
#include "Particles/Common/ParticleHelper.h"
#include "Core/EngineTypes.h"
#include "Component/ParticleSystemComponent.h"
#include "Math/Vector.h"

#include <utility>
#include <Core/Log.h>

FParticleEmitterInstance::~FParticleEmitterInstance()
{
	delete[] ParticleData;
	ParticleData = nullptr;

	delete[] ParticleIndices;
	ParticleIndices = nullptr;

	delete[] InstanceData;
	InstanceData = nullptr;
	//BurstFired.Empty();
}

void FParticleEmitterInstance::Init(UParticleSystemComponent* InComponent, UParticleEmitter* InTemplate)
{
	if(!InComponent || !InTemplate) return;

	OwnerComponent = InComponent;
	EmitterTemplate = InTemplate;
	CurrentLODLevel = InTemplate->GetLODLevel(CurrentLODLevelIndex);

	MaxActiveParticles = EmitterTemplate->GetMaxActiveParticles();

	PayloadOffset = sizeof(FBaseParticle);

	ParticleSize = std::max<size_t>(
		static_cast<size_t>(EmitterTemplate->GetParticleSize()),sizeof(FBaseParticle));

	ParticleStride = ParticleSize;

	ParticleData = new uint8[ParticleStride * MaxActiveParticles];
	ParticleIndices = new uint16[MaxActiveParticles];

	memset(ParticleData, 0, ParticleStride * MaxActiveParticles);
	for (int32 i = 0; i < MaxActiveParticles; ++i)
	{
		ParticleIndices[i] = static_cast<uint16>(i);
	}

	ActiveParticles = 0;
	ParticleCounter = 0;
	SpawnFraction = 0.0f;
	bFirstTime = true;
	bEnabled = true;
	LoopCount = 0;
	EmitterTime = 0.0f;
	LastDeltaTime = 0.0f;
}

void FParticleEmitterInstance::Tick(float DeltaTime)
{
	if (!CurrentLODLevel)
		return;

	if(!bEnabled) return;

	//life타임 없는 애들 삭제 하는 로직
	KillExpiredParticles();

	ResetParticleParameters(DeltaTime);

	for (UParticleModule* Module : CurrentLODLevel->GetUpdateModules())
	{
		if (Module && Module->IsEnabled())
			Module->Update(this, DeltaTime);
	}

	//시간되었으면 spawn하는 로직
	Tick_SpawnParticles(DeltaTime);
	//if (ActiveParticles > 0) UpdateBoundingBox(DeltaTime);

	ProcessEvents();
}

void FParticleEmitterInstance::SpawnParticles(int32 Count, float StartTime, float Increment, const FVector& InitialLocation, const FVector& InitialVelocity)
{
	for (int32 i = 0; i < Count; ++i)
	{
		if (ActiveParticles >= MaxActiveParticles)
			break;

		const int32 ActiveIndex = ActiveParticles;

		DECLARE_PARTICLE_PTR(ActiveIndex);

		PreSpawn(Particle, InitialLocation, InitialVelocity);

		const float SpawnTime = StartTime + Increment * i;

		for (UParticleModule* Module : CurrentLODLevel->GetSpawnModules())
		{
			if (Module)
			{
				Module->Spawn(this, Particle, SpawnTime);
			}
		}

		PostSpawn(Particle, SpawnTime);
	}
}

void FParticleEmitterInstance::KillParticle(int32 Index)
{
	if (Index < ActiveParticles)
	{
		int32 KillIndex = ParticleIndices[Index];

		for (int32 i = Index; i < ActiveParticles - 1; i++)
		{
			ParticleIndices[i] = ParticleIndices[i + 1];
		}
		ParticleIndices[ActiveParticles - 1] = KillIndex;
		ActiveParticles--;
	}
}

void FParticleEmitterInstance::KillAllParticles()
{
	ActiveParticles = 0;
}

void FParticleEmitterInstance::Reset()
{
	EmitterTime = 0;
	LastDeltaTime = 0;
	LoopCount = 0;
	ParticleCounter = 0;
	bEnabled = true;
	KillAllParticles();
}

void FParticleEmitterInstance::ResetParticleParameters(float DeltaTime)
{
	if (!CurrentLODLevel)
		return;

	if (!EmitterTemplate)
		return;

	if (!ParticleData)
		return;

	for (int32 ParticleIndex = 0; ParticleIndex < ActiveParticles; ++ParticleIndex)
	{
		const int32 RealIndex = ParticleIndices[ParticleIndex];

		uint8* ParticleBase =
			ParticleData + ParticleStride * RealIndex;

		FBaseParticle* Particle =
			reinterpret_cast<FBaseParticle*>(ParticleBase);

		// 모듈 업데이트 전에 기본 상태로 리셋
		Particle->Velocity = Particle->BaseVelocity;
		Particle->Size = Particle->BaseSize;
		Particle->RotationRate = Particle->BaseRotationRate;
		Particle->Color = Particle->BaseColor;

		if (Particle->Lifetime > 0.0f)
		{
			Particle->RelativeTime += DeltaTime / Particle->Lifetime;
		}
	}
}

void FParticleEmitterInstance::Tick_SpawnParticles(float DeltaTime)
{
	if (!CurrentLODLevel || !bEnabled)
		return;

	if (EmitterTime < 0.0f)
		return;

	float SpawnRate = 0.0f;
	int32 BurstCount = 0;

	for (UParticleModule* Module : CurrentLODLevel->GetSpawnModules())
	{
		if (!Module || !Module->IsEnabled())
			continue;

		if (Module->GetModuleType() == EParticleModuleType::PMT_Spawn)
		{
			UParticleModuleSpawn* SpawnModule = static_cast<UParticleModuleSpawn*>(Module);
			SpawnRate += std::max(0.0f, SpawnModule->GetSpawnRate());

			if (bFirstTime)
			{
				//BurstCount += SpawnModule->GetBurstCount();
			}
		}
	}

	const float NewLeftover = SpawnFraction + DeltaTime * SpawnRate;
	int32 SpawnCount = floor(NewLeftover);
	float NewSpawnFraction = NewLeftover - SpawnCount;

	const int32 AvailableSlots = MaxActiveParticles - ActiveParticles;
	const int32 TotalSpawnCount = std::min(SpawnCount + BurstCount, AvailableSlots);

	if (TotalSpawnCount > 0)
	{
		const float Increment = SpawnRate > 0.0f ? 1.0f / SpawnRate : 0.0f;
		const float StartTime = DeltaTime + SpawnFraction * Increment - Increment;

		const FVector InitialLocation = OwnerComponent
			? OwnerComponent->GetWorldLocation()
			: FVector::ZeroVector;

		SpawnParticles(
			TotalSpawnCount,
			StartTime,
			Increment,
			InitialLocation,
			FVector::ZeroVector
		);
	}

	bFirstTime = false;
	SpawnFraction = NewSpawnFraction;
}

FDynamicEmitterDataBase* FParticleEmitterInstance::CreateDynamicEmitterData(FDynamicEmitterReplayDataBase& Data)
{
	return nullptr;
}

void FParticleEmitterInstance::PreSpawn(FBaseParticle& Particle, const FVector& InitialLocation, const FVector& InitialVelocity)
{
	Particle.Location = InitialLocation;
	Particle.Velocity = InitialVelocity;
	Particle.BaseVelocity = InitialVelocity;
	Particle.RelativeTime = 0.0f;
	Particle.Lifetime = 1.0f;
	Particle.Color = FColor::White();
	Particle.BaseColor = Particle.Color;
	Particle.Size = FVector(1.0f, 1.0f, 1.0f);
	Particle.BaseSize = Particle.Size;
	Particle.RotationRate = 0.0f;
	Particle.BaseRotationRate = Particle.RotationRate;

	for (UParticleModule* Module : CurrentLODLevel->GetSpawnModules())
	{
		if (Module && Module->IsEnabled())
			Module->PreSpawn(this, Particle);
	}
}

void FParticleEmitterInstance::PostSpawn(FBaseParticle& Particle, float SpawnTime)
{
	/*if (CurrentLODLevel->GetRequiredModule()->bUseLocalSpace == false)
	{
		if (FVector::DistSquared(OldLocation, Location) > 1.f)
		{
			Particle->Location += InterpolationPercentage * (OldLocation - Location);
		}
	}*/
	// Offset caused by any velocity
	Particle.Location += FVector(Particle.Velocity) * SpawnTime;

	++ActiveParticles;
	++ParticleCounter;
}

void FParticleEmitterInstance::KillExpiredParticles()
{
    for (int32 i = ActiveParticles - 1; i >= 0; --i)
    {
		DECLARE_PARTICLE_PTR(i);
        if (Particle.RelativeTime >= 1.0f)
        {
            KillParticle(i);
        }
    }
}

void FParticleEmitterInstance::ProcessEvents()
{
	
}

FBaseParticle& FParticleEmitterInstance::GetParticle(int32 index)
{
	return *reinterpret_cast<FBaseParticle*>(ParticleData + ParticleStride * ParticleIndices[index]);
}
