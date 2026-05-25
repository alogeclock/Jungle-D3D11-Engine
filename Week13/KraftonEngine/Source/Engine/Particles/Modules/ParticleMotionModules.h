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
    virtual void Serialize(FArchive &Ar) override;
    virtual void CacheModuleValues() override;
    virtual void Spawn(FParticleEmitterInstance *Owner, FBaseParticle &Particle, float SpawnTime) override;

    UDistributionFloat *GetRotationDist() const
    {
        return RotationDist;
    }

  private:
    UDistributionFloat *RotationDist = nullptr;
    FRawDistributionFloat RawRotation = FRawDistributionFloat::MakeConstant(0.f);
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
    virtual void Serialize(FArchive &Ar) override;
    virtual void CacheModuleValues() override;
    virtual void Spawn(FParticleEmitterInstance *Owner, FBaseParticle &Particle, float SpawnTime) override;
    virtual void Update(FParticleEmitterInstance *Owner, float DeltaTime) override;

    UDistributionFloat *GetRotationRateDist() const
    {
        return RotationRateDist;
    }

  private:
    UDistributionFloat *RotationRateDist = nullptr;
    FRawDistributionFloat RawRotationRate = FRawDistributionFloat::MakeConstant(0.f);
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

  private:
    FVector Acceleration = FVector::ZeroVector;      // 매 프레임 적용할 가속도
    FVector ConstAcceleration = FVector::ZeroVector; // 고정 가속도
    float   Drag = 0.0f;                             // 속도 감쇠 계수
    bool    bUseAccelerationOverLife = false;        // 수명 기반 가속도 변화 사용 여부
};

// ─────────────────────────────────────────────────────────────────────────────
// UParticleModuleAttractor
// ─────────────────────────────────────────────────────────────────────────────

UCLASS()
class UParticleModuleAttractor : public UParticleModule
{
  public:
    GENERATED_BODY(UParticleModuleAttractor)

    virtual EParticleModuleType GetModuleType() const override
    {
        return EParticleModuleType::PMT_Attractor;
    }
    virtual EParticleModuleUpdatePhase GetUpdatePhase() const override
    {
        return EParticleModuleUpdatePhase::PMUP_Update;
    }
    virtual EParticleModuleClass GetModuleClass() const override
    {
        return EParticleModuleClass::Attractor;
    }
    virtual void Serialize(FArchive &Ar) override;

  private:
    FVector TargetLocation = FVector::ZeroVector; // 흡인 목표 위치
    float   Strength = 0.0f;                      // 흡인 강도
    float   Radius = 0.0f;                        // 흡인 영향 반경
};

// ─────────────────────────────────────────────────────────────────────────────
// UParticleModuleOrbit
// ─────────────────────────────────────────────────────────────────────────────

UCLASS()
class UParticleModuleOrbit : public UParticleModule
{
  public:
    GENERATED_BODY(UParticleModuleOrbit)

    virtual EParticleModuleType GetModuleType() const override
    {
        return EParticleModuleType::PMT_Orbit;
    }
    virtual EParticleModuleUpdatePhase GetUpdatePhase() const override
    {
        return EParticleModuleUpdatePhase::PMUP_Update;
    }
    virtual EParticleModuleClass GetModuleClass() const override
    {
        return EParticleModuleClass::Orbit;
    }
    virtual void Serialize(FArchive &Ar) override;

  private:
    FVector Offset       = FVector::ZeroVector; // 렌더링 위치 오프셋
    FVector RotationRate = FVector::ZeroVector; // 궤도 회전 속도
};
