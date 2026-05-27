/**
 * @file ParticleRenderExpressionModules.h
 * @brief Particle 렌더링 표현 확장 Module 정의.
 *
 * 포함 클래스:
 * - UParticleModuleSubUVBase: Cascade SubUV 공통 base
 * - UParticleModuleSubImageIndex: RelativeTime 기반 SubImage Index
 * - UParticleModuleSubUVMovie: FPS 기반 SubUV Movie
 */

#pragma once

#include "ParticleEventModules.h"
#include "ParticleRenderExpressionModules.generated.h"

/** Cascade SubUV 공통 base. Add Module 메뉴에는 노출하지 않는다. */
UCLASS()
class UParticleModuleSubUVBase : public UParticleModule
{
  public:
    GENERATED_BODY(UParticleModuleSubUVBase)

    virtual EParticleModuleType        GetModuleType() const override { return EParticleModuleType::PMT_SubUV; }
    virtual EParticleModuleUpdatePhase GetUpdatePhase() const override { return EParticleModuleUpdatePhase::PMUP_SpawnAndUpdate; }
    virtual bool SupportsRandomSeed() const override { return true; }
    virtual void Serialize(FArchive& Ar) override;

    bool UsesRealTime() const { return bUseRealTime; }

  protected:
    UParticleModuleRequired* GetRequiredModule(FParticleEmitterInstance* Owner) const;
    EParticleSubUVInterpMethod GetInterpolationMethod(FParticleEmitterInstance* Owner) const;
    int32 GetSubImageCount(FParticleEmitterInstance* Owner) const;
    int32 ClampFrameIndex(FParticleEmitterInstance* Owner, int32 InFrame) const;
    int32 WrapFrameIndex(FParticleEmitterInstance* Owner, int32 InFrame) const;
    float WrapFrameValue(FParticleEmitterInstance* Owner, float InFrame) const;
    float GetDeltaTime(FParticleEmitterInstance* Owner, float DeltaTime) const;
    void SetParticleSubUVFrames(FBaseParticle& Particle, float FrameA, float FrameB, float Lerp) const;
    void SetParticleSequentialSubUV(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float ImageIndex, bool bBlend, bool bLoop) const;
    float SelectRandomFrame(FParticleEmitterInstance* Owner) const;

    UPROPERTY(Edit, Category="Realtime", DisplayName="Use Real Time")
    bool bUseRealTime = false;
};

/** RelativeTime 기반 SubImage Index 모듈 */
UCLASS()
class UParticleModuleSubImageIndex : public UParticleModuleSubUVBase
{
  public:
    GENERATED_BODY(UParticleModuleSubImageIndex)

    virtual EParticleModuleClass GetModuleClass() const override { return EParticleModuleClass::SubImageIndex; }
    virtual void Serialize(FArchive& Ar) override;
    virtual void CacheModuleValues() override;
    virtual void Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime, int32 ModuleOffset = INDEX_NONE) override;
    virtual void Update(
        FParticleEmitterInstance* Owner,
        float DeltaTime,
        int32 ModuleOffset = INDEX_NONE,
        TArray<FParticleEventData>* OutEventQueue = nullptr) override;

    UDistributionFloat* GetSubImageIndexDist() const { return SubImageIndexDist; }

  private:
    void InitializeDefaultSubImageIndexDistribution();
    void ApplySubImageIndex(FParticleEmitterInstance* Owner, FBaseParticle& Particle);
    void InitializeRandomSubImage(FParticleEmitterInstance* Owner, FBaseParticle& Particle);
    void ApplyRandomSubImage(FParticleEmitterInstance* Owner, FBaseParticle& Particle);

    UPROPERTY(Edit, Category="Particle", DisplayName="Sub Image Index", Type=Distribution, Class=UDistributionFloat)
    UDistributionFloat* SubImageIndexDist = nullptr;
    FRawDistributionFloat RawSubImageIndex = FRawDistributionFloat::MakeConstant(0.0f);
};

/** FPS 기반 SubUV Movie 모듈 */
UCLASS()
class UParticleModuleSubUVMovie : public UParticleModuleSubUVBase
{
  public:
    GENERATED_BODY(UParticleModuleSubUVMovie)

    virtual EParticleModuleClass GetModuleClass() const override { return EParticleModuleClass::SubUVMovie; }
    virtual void Serialize(FArchive& Ar) override;
    virtual void CacheModuleValues() override;
    virtual void Spawn(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float SpawnTime, int32 ModuleOffset = INDEX_NONE) override;
    virtual void Update(
        FParticleEmitterInstance* Owner,
        float DeltaTime,
        int32 ModuleOffset = INDEX_NONE,
        TArray<FParticleEventData>* OutEventQueue = nullptr) override;

    UDistributionFloat* GetFrameRateDist() const { return FrameRateDist; }

  private:
    void InitializeDefaultFrameRateDistribution();
    int32 ResolveStartingFrame(FParticleEmitterInstance* Owner);
    void ApplyMovieFrame(FParticleEmitterInstance* Owner, FBaseParticle& Particle, float DeltaTime);

    UPROPERTY(Edit, Category="Particle", DisplayName="Frame Rate", Type=Distribution, Class=UDistributionFloat)
    UDistributionFloat* FrameRateDist = nullptr;
    FRawDistributionFloat RawFrameRate = FRawDistributionFloat::MakeConstant(30.0f);

    UPROPERTY(Edit, Category="Flipbook", DisplayName="Starting Frame", Min=0, Max=100000, Speed=1.0)
    int32 StartingFrame = 1;
    UPROPERTY(Edit, Category="Flipbook", DisplayName="Use Emitter Time")
    bool bUseEmitterTime = false;
};

