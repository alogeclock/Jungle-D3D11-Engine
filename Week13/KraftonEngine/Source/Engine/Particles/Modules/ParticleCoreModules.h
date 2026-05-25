/**
 * @file ParticleCoreModules.h
 * @brief 기본 Particle Module 정의.
 *
 * 각 모듈은 에디터 측 UDistribution* (커브 편집 가능) 과
 * 런타임 측 FRawDistribution (Baked 테이블) 을 함께 보유한다.
 * CacheModuleValues()를 호출하면 UDistribution → FRawDistribution 동기화가 된다.
 */

#pragma once

#include "ParticleModules.h"
#include "Particles/Common/ParticleDistributionTypes.h"
#include "ParticleCoreModules.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// UParticleModuleRequired
// ─────────────────────────────────────────────────────────────────────────────

UCLASS()
class UParticleModuleRequired : public UParticleModule
{
  public:
    GENERATED_BODY(UParticleModuleRequired)

    virtual EParticleModuleType        GetModuleType() const override { return EParticleModuleType::PMT_Required; }
    virtual EParticleModuleUpdatePhase GetUpdatePhase() const override { return EParticleModuleUpdatePhase::PMUP_Spawn; }
    virtual EParticleModuleClass       GetModuleClass() const override { return EParticleModuleClass::Required; }
    virtual void Serialize(FArchive& Ar) override;

    EParticleEmitterType GetEmitterType() const { return EmitterType; }
    void                 SetEmitterType(EParticleEmitterType InType) { EmitterType = InType; }

    UMaterial *GetMaterial() const { return Material; }
    void       SetMaterial(UMaterial *InMaterial) { Material = InMaterial; }

    EParticleSortMode GetSortMode() const { return SortMode; }
    void              SetSortMode(EParticleSortMode InSortMode) { SortMode = InSortMode; }

  private:
    EParticleEmitterType EmitterType = EParticleEmitterType::PET_Sprite; // 기본 Emitter 타입
    UMaterial           *Material = nullptr;                             // Particle 렌더링 Material
    EParticleSortMode    SortMode = EParticleSortMode::PSM_None;         // Particle 정렬 방식
    int32                TranslucencySortPriority = 0;                   // Translucent Pass 정렬 우선순위
};

// ─────────────────────────────────────────────────────────────────────────────
// UParticleModuleSpawn
// ─────────────────────────────────────────────────────────────────────────────

UCLASS()
class UParticleModuleSpawn : public UParticleModule
{
  public:
    GENERATED_BODY(UParticleModuleSpawn)

    virtual EParticleModuleType        GetModuleType() const override { return EParticleModuleType::PMT_Spawn; }
    virtual EParticleModuleUpdatePhase GetUpdatePhase() const override { return EParticleModuleUpdatePhase::PMUP_Spawn; }
    virtual EParticleModuleClass       GetModuleClass() const override { return EParticleModuleClass::Spawn; }
    virtual void Serialize(FArchive& Ar) override;

    float GetSpawnRate() const { return SpawnRate; }
    void  SetSpawnRate(float InSpawnRate) { SpawnRate = InSpawnRate; }

    int32 GetBurstCount() const { return BurstCount; }
    void  SetBurstCount(int32 InBurstCount) { BurstCount = InBurstCount; }

  private:
    float SpawnRate  = 10.0f; // 초당 Particle 생성 수
    int32 BurstCount = 0;    // 순간 Spawn 개수
};

// ─────────────────────────────────────────────────────────────────────────────
// UParticleModuleLifetime
// ─────────────────────────────────────────────────────────────────────────────

UCLASS()
class UParticleModuleLifetime : public UParticleModule
{
  public:
    GENERATED_BODY(UParticleModuleLifetime)

    virtual EParticleModuleType        GetModuleType() const override { return EParticleModuleType::PMT_Lifetime; }
    virtual EParticleModuleUpdatePhase GetUpdatePhase() const override { return EParticleModuleUpdatePhase::PMUP_Spawn; }
    virtual EParticleModuleClass       GetModuleClass() const override { return EParticleModuleClass::Lifetime; }
    virtual void Serialize(FArchive& Ar) override;
    virtual void CacheModuleValues() override;
    virtual void Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime) override;

    UDistributionFloat* GetLifetimeDist() const { return LifetimeDist; }

  private:
    // 에디터 측: 커브/범위 편집 가능
    UDistributionFloat* LifetimeDist = nullptr;
    // 런타임 측: Baked 테이블
    FRawDistributionFloat RawLifetime = FRawDistributionFloat::MakeConstant(1.0f);
};

// ─────────────────────────────────────────────────────────────────────────────
// UParticleModuleLocation
// ─────────────────────────────────────────────────────────────────────────────

UCLASS()
class UParticleModuleLocation : public UParticleModule
{
  public:
    GENERATED_BODY(UParticleModuleLocation)

    virtual EParticleModuleType        GetModuleType() const override { return EParticleModuleType::PMT_Location; }
    virtual EParticleModuleUpdatePhase GetUpdatePhase() const override { return EParticleModuleUpdatePhase::PMUP_Spawn; }
    virtual EParticleModuleClass       GetModuleClass() const override { return EParticleModuleClass::Location; }
    virtual void Serialize(FArchive& Ar) override;
    virtual void CacheModuleValues() override;
    virtual void Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime) override;

    UDistributionVector* GetLocationDist() const { return LocationDist; }

  private:
    UDistributionVector* LocationDist = nullptr;
    FRawDistributionVector RawLocation;

    float SphereRadius   = 0.0f;
    float CylinderRadius = 0.0f;
    float CylinderHeight = 0.0f;
};

// ─────────────────────────────────────────────────────────────────────────────
// UParticleModuleVelocity
// ─────────────────────────────────────────────────────────────────────────────

UCLASS()
class UParticleModuleVelocity : public UParticleModule
{
  public:
    GENERATED_BODY(UParticleModuleVelocity)

    virtual EParticleModuleType        GetModuleType() const override { return EParticleModuleType::PMT_Velocity; }
    virtual EParticleModuleUpdatePhase GetUpdatePhase() const override { return EParticleModuleUpdatePhase::PMUP_Spawn; }
    virtual EParticleModuleClass       GetModuleClass() const override { return EParticleModuleClass::Velocity; }
    virtual void Serialize(FArchive& Ar) override;
    virtual void CacheModuleValues() override;
    virtual void Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime) override;

    UDistributionVector* GetVelocityDist() const { return VelocityDist; }

  private:
    UDistributionVector* VelocityDist = nullptr;
    FRawDistributionVector RawVelocity;
};

// ─────────────────────────────────────────────────────────────────────────────
// UParticleModuleColor
// ─────────────────────────────────────────────────────────────────────────────

UCLASS()
class UParticleModuleColor : public UParticleModule
{
  public:
    GENERATED_BODY(UParticleModuleColor)

    virtual EParticleModuleType        GetModuleType() const override { return EParticleModuleType::PMT_Color; }
    virtual EParticleModuleUpdatePhase GetUpdatePhase() const override { return EParticleModuleUpdatePhase::PMUP_SpawnAndUpdate; }
    virtual EParticleModuleClass       GetModuleClass() const override { return EParticleModuleClass::Color; }
    virtual void Serialize(FArchive& Ar) override;
    virtual void CacheModuleValues() override;
    virtual void Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime) override;
    virtual void Update(FParticleEmitterInstance* Owner, float DeltaTime) override;

    UDistributionLinearColor* GetColorDist() const { return ColorDist; }

  private:
    UDistributionLinearColor* ColorDist = nullptr;
    FRawDistributionLinearColor RawColor = FRawDistributionLinearColor::MakeConstant(FLinearColor::White());
};

// ─────────────────────────────────────────────────────────────────────────────
// UParticleModuleSize
// ─────────────────────────────────────────────────────────────────────────────

UCLASS()
class UParticleModuleSize : public UParticleModule
{
  public:
    GENERATED_BODY(UParticleModuleSize)

    virtual EParticleModuleType        GetModuleType() const override { return EParticleModuleType::PMT_Size; }
    virtual EParticleModuleUpdatePhase GetUpdatePhase() const override { return EParticleModuleUpdatePhase::PMUP_SpawnAndUpdate; }
    virtual EParticleModuleClass       GetModuleClass() const override { return EParticleModuleClass::Size; }
    virtual void Serialize(FArchive& Ar) override;
    virtual void CacheModuleValues() override;
    virtual void Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime) override;
    virtual void Update(FParticleEmitterInstance* Owner, float DeltaTime) override;

    UDistributionVector* GetSizeDist() const { return SizeDist; }

  private:
    UDistributionVector* SizeDist = nullptr;
    FRawDistributionVector RawSize = FRawDistributionVector::MakeConstant(FVector(1.f, 1.f, 1.f));
};
