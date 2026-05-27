/**
 * @file ParticleMotionModules.h
 * @brief Particle 운동 / 회전 / 힘 관련 Module 정의.
 */

#pragma once

#include "ParticleCoreModules.h"
#include "ParticleMotionModules.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// UParticleModuleRotation
// ─────────────────────────────────────────────────────────────────────────────

UCLASS()
class UParticleModuleRotation : public UParticleModule
{
  public:
    GENERATED_BODY(UParticleModuleRotation)

    virtual EParticleModuleType GetModuleType() const override
    {
        return EParticleModuleType::PMT_Rotation;
    }
    virtual EParticleModuleUpdatePhase GetUpdatePhase() const override
    {
        return EParticleModuleUpdatePhase::PMUP_Spawn;
    }
    virtual EParticleModuleClass GetModuleClass() const override
    {
        return EParticleModuleClass::Rotation;
    }
    virtual bool SupportsRandomSeed() const override
    {
        return true;
    }
    virtual void Serialize(FArchive &Ar) override;
    virtual void CacheModuleValues() override;
    virtual uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) const override;
    virtual void Spawn(FParticleEmitterInstance *Owner, FBaseParticle &Particle, float SpawnTime, int32 ModuleOffset = INDEX_NONE) override;

    UDistributionFloat* GetRotationDist() const
    {
        return RotationDist;
    }

  private:
    UPROPERTY(Edit, Category="Particle", DisplayName="Rotation", Type=Distribution, Class=UDistributionFloat)
    UDistributionFloat* RotationDist = nullptr;
    FRawDistributionFloat RawRotation = FRawDistributionFloat::MakeConstant(0.f);
    UPROPERTY(Edit, Category="Particle", DisplayName="Initial Mesh Rotation")
    FRotator InitialMeshRotation; // Mesh Particle 초기 회전값
};

// ─────────────────────────────────────────────────────────────────────────────
// UParticleModuleRotationRate
// ─────────────────────────────────────────────────────────────────────────────

UCLASS()
class UParticleModuleRotationRate : public UParticleModule
{
  public:
    GENERATED_BODY(UParticleModuleRotationRate)

    virtual EParticleModuleType GetModuleType() const override
    {
        return EParticleModuleType::PMT_RotationRate;
    }
    virtual EParticleModuleUpdatePhase GetUpdatePhase() const override
    {
        return EParticleModuleUpdatePhase::PMUP_SpawnAndUpdate;
    }
    virtual EParticleModuleClass GetModuleClass() const override
    {
        return EParticleModuleClass::RotationRate;
    }
    virtual bool SupportsRandomSeed() const override
    {
        return true;
    }
    virtual void Serialize(FArchive &Ar) override;
    virtual void CacheModuleValues() override;
    virtual void Spawn(FParticleEmitterInstance *Owner, FBaseParticle &Particle, float SpawnTime, int32 ModuleOffset = INDEX_NONE) override;
    virtual void Update(
        FParticleEmitterInstance *Owner,
        float DeltaTime,
        int32 ModuleOffset = INDEX_NONE,
        TArray<FParticleEventData>* OutEventQueue = nullptr) override;

    UDistributionFloat* GetRotationRateDist() const
    {
        return RotationRateDist;
    }

  private:
    UPROPERTY(Edit, Category="Particle", DisplayName="Rotation Rate", Type=Distribution, Class=UDistributionFloat)
    UDistributionFloat* RotationRateDist = nullptr;
    FRawDistributionFloat RawRotationRate = FRawDistributionFloat::MakeConstant(0.f);
    UPROPERTY(Edit, Category="Particle", DisplayName="Mesh Rotation Rate")
    FVector MeshRotationRate; // Mesh Particle 회전 속도
};

// ─────────────────────────────────────────────────────────────────────────────
// UParticleModuleAcceleration
// ─────────────────────────────────────────────────────────────────────────────

UCLASS()
class UParticleModuleAcceleration : public UParticleModule
{
  public:
    GENERATED_BODY(UParticleModuleAcceleration)

    virtual EParticleModuleType GetModuleType() const override
    {
        return EParticleModuleType::PMT_Acceleration;
    }
    virtual EParticleModuleUpdatePhase GetUpdatePhase() const override
    {
        return EParticleModuleUpdatePhase::PMUP_SpawnAndUpdate;
    }
    virtual EParticleModuleClass GetModuleClass() const override
    {
        return EParticleModuleClass::Acceleration;
    }
    virtual void Serialize(FArchive &Ar) override;
    virtual uint32 RequiredBytes(UParticleModuleTypeDataBase* TypeData) const override;
    virtual void Spawn(FParticleEmitterInstance *Owner, FBaseParticle &Particle, float SpawnTime, int32 ModuleOffset = INDEX_NONE) override;
    virtual void Update(
        FParticleEmitterInstance *Owner,
        float DeltaTime,
        int32 ModuleOffset = INDEX_NONE,
        TArray<FParticleEventData>* OutEventQueue = nullptr) override;

  private:
    UPROPERTY(Edit, Category="Particle", DisplayName="Acceleration")
    FVector Acceleration = FVector::ZeroVector;      // 매 프레임 적용할 가속도
    UPROPERTY(Edit, Category="Particle", DisplayName="Const Acceleration")
    FVector ConstAcceleration = FVector::ZeroVector; // 고정 가속도
    UPROPERTY(Edit, Category="Particle", DisplayName="Drag", Min=0.0, Max=1000.0, Speed=0.01)
    float   Drag = 0.0f;                             // 속도 감쇠 계수
    UPROPERTY(Edit, Category="Particle", DisplayName="Use Acceleration Over Life")
    bool    bUseAccelerationOverLife = false;        // 수명 기반 가속도 변화 사용 여부
};

