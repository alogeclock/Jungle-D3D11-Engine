/**
 * @file ParticleAsset.h
 * @brief ParticleSystem Asset 계층 정의.
 *
 * 포함 클래스:
 * - UParticleSystem: 여러 Emitter를 묶는 Particle System Asset
 * - UParticleEmitter: 개별 Emitter Asset
 * - UParticleLODLevel: Emitter의 LOD별 Module 설정
 */

#pragma once

#include "Object/Object.h"
#include "Particles/Assets/ParticleTypeData.h"
#include "ParticleAsset.generated.h"

/** LOD 레벨별 시스템 설정 (LODDistances 배열과 1:1 대응) */
USTRUCT()
struct FParticleSystemLOD
{
    GENERATED_BODY(FParticleSystemLOD)
    // 향후 LOD별 품질 설정 항목을 여기에 추가
};

inline FArchive& operator<<(FArchive& Ar, FParticleSystemLOD&)
{
    return Ar; // 현재 직렬화할 멤버 없음
}

/** LOD별 Module, TypeData, 실행 캐시를 보관하는 클래스 */
UCLASS()
class UParticleLODLevel : public UObject
{
  public:
    GENERATED_BODY(UParticleLODLevel)

    int32 GetLevel() const { return Level; }
    void  SetLevel(int32 InLevel) { Level = InLevel; }

    bool IsEnabled() const { return bEnabled; }
    void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

    UParticleModuleRequired *GetRequiredModule() const { return RequiredModule; }
    void                     SetRequiredModule(UParticleModuleRequired *InModule);

    UParticleModuleTypeDataBase *GetTypeDataModule() const { return TypeDataModule; }
    void                         SetTypeDataModule(UParticleModuleTypeDataBase *InModule);
    void                         SyncRequiredModuleToTypeData();
    void                         SyncTypeDataModuleToRequired();

    const TArray<UParticleModule *> &GetModules() const { return Modules; }
    const TArray<UParticleModule *> &GetSpawnModules() const { return SpawnModules; }
    const TArray<UParticleModule *> &GetUpdateModules() const { return UpdateModules; }

    void AddModule(UParticleModule *InModule); // 일반 Module 추가
    void RemoveModule(int32 Index) { Modules.erase(Modules.begin() + Index); }
    bool MoveModule(int32 FromIndex, int32 ToIndex);
    void CacheModules();                       // Spawn / Update 실행 캐시 구성
    virtual void Serialize(FArchive& Ar) override;

  private:
    UPROPERTY(Edit, Category="Particle")
    int32 Level = 0;       // LOD 단계
    UPROPERTY(Edit, Category="Particle")
    bool  bEnabled = true; // LOD 사용 여부

    UParticleModuleRequired     *RequiredModule = nullptr; // 필수 Emitter 설정
    TArray<UParticleModule *>    Modules;                  // 전체 일반 Module 목록
    UParticleModuleTypeDataBase *TypeDataModule = nullptr; // Emitter 타입 설정

    TArray<UParticleModule *> SpawnModules;  // Spawn 실행 캐시
    TArray<UParticleModule *> UpdateModules; // Update 실행 캐시
};

/** 단일 Particle Emitter Asset */
UCLASS()
class UParticleEmitter : public UObject
{
  public:
    GENERATED_BODY(UParticleEmitter)

    FName GetEmitterName() const { return EmitterName; }
    void  SetEmitterName(const FName &InName) { EmitterName = InName; }

    EParticleEmitterRenderMode GetEmitterRenderMode() const { return EmitterRenderMode; }
    void                       SetEmitterRenderMode(EParticleEmitterRenderMode InMode) { EmitterRenderMode = InMode; }

    FColor GetEmitterEditorColor() const { return EmitterEditorColor; }
    void   SetEmitterEditorColor(const FColor &InColor) { EmitterEditorColor = InColor; }

    int32 GetInitialAllocationCount() const { return InitialAllocationCount; }
    void  SetInitialAllocationCount(int32 InCount) { InitialAllocationCount = InCount; }

    float GetMediumDetailSpawnRateScale() const { return MediumDetailSpawnRateScale; }
    void  SetMediumDetailSpawnRateScale(float InScale) { MediumDetailSpawnRateScale = InScale; }

    bool IsCollapsed() const { return bCollapsed; }
    void SetCollapsed(bool bInCollapsed) { bCollapsed = bInCollapsed; }

    const TArray<UParticleLODLevel *> &GetLODLevels() const { return LODLevels; }

    UParticleLODLevel *GetLODLevel(int32 Index) const { return (Index >= 0 && Index < (int32)LODLevels.size()) ? LODLevels[Index] : nullptr; }

    int32 GetLODLevelCount() const { return static_cast<int32>(LODLevels.size()); }
    void AddLODLevel(UParticleLODLevel *InLODLevel);
    void InsertLODLevel(int32 Index, UParticleLODLevel *InLODLevel);
    void RemoveLODLevel(int32 Index);
    void RefreshLODLevelIndices();

    void CacheEmitterModuleInfo(); // Emitter Module 정보 캐싱
    virtual void Serialize(FArchive& Ar) override;

    int32 GetParticleSize() const { return ParticleSize; }
    int32 GetModulePayloadOffset(const UParticleModule* Module) const;

    int32 GetMaxActiveParticles() const { return MaxActiveParticles; }
    void  SetMaxActiveParticles(int32 InMaxCount) { MaxActiveParticles = InMaxCount; }

  private:
    UPROPERTY(Edit, Category="Particle", DisplayName="Emitter Name")
    FName EmitterName; // Emitter 이름

    UPROPERTY(Edit, Category="Particle", DisplayName="Render Mode")
    EParticleEmitterRenderMode EmitterRenderMode = EParticleEmitterRenderMode::ERM_Normal; // Emitter 렌더 표시 모드

    FColor EmitterEditorColor = FColor::White(); // 에디터 / 디버그 표시 색상

    UPROPERTY(Edit, Category="Particle", DisplayName="Initial Allocation", Min=0, Max=100000, Speed=1.0)
    int32 InitialAllocationCount = 0; // 초기화 시 미리 할당할 Particle 수

    UPROPERTY(Edit, Category="Particle", DisplayName="Medium Detail Spawn Rate Scale", Min=0.0, Max=10.0, Speed=0.01)
    float MediumDetailSpawnRateScale = 1.0f; // Medium / Low Detail에서 SpawnRate를 줄이는 배율

    UPROPERTY(Edit, Category="Particle", DisplayName="Collapsed")
    bool bCollapsed = false; // 에디터 Emitter List 접힘 상태

    TArray<UParticleLODLevel *> LODLevels; // Emitter LOD 목록

    int32 ParticleSize = 0; // Particle 1개 메모리 크기
    TMap<const UParticleModule*, int32> ModuleOffsetMap; // Module별 payload 시작 offset

    UPROPERTY(Edit, Category="Particle", DisplayName="Max Active Particles", Min=1, Max=100000, Speed=1.0)
    int32 MaxActiveParticles = 1000; // 최대 활성 Particle 수
};

/** 여러 Emitter를 묶는 Particle System Asset */
UCLASS()
class UParticleSystem : public UObject
{
  public:
    GENERATED_BODY(UParticleSystem)

    const TArray<UParticleEmitter *> &GetEmitters() const { return Emitters; }

    UParticleEmitter *GetEmitter(int32 Index) const { return Index < (int32)Emitters.size() ? Emitters[Index] : nullptr; }

    void AddEmitter(UParticleEmitter *InEmitter) { Emitters.push_back(InEmitter); }
    void RemoveEmitter(int32 Index) { Emitters.erase(Emitters.begin() + Index); }

    void CacheSystemModuleInfo(); // 전체 Emitter Module 정보 캐싱
    virtual void Serialize(FArchive& Ar) override;

    UPROPERTY(Edit, Category="LOD", DisplayName="LOD Distance Check Time", Min=0.0, Max=10.0, Speed=0.01)
    float LODDistanceCheckTime = 0.25f; // LOD 전환 거리 재검사 주기 (초)

    UPROPERTY(Edit, FixedSize, Category="LOD", DisplayName="LOD Distances")
    TArray<float> LODDistances; // LOD 전환 거리 목록 (추가/삭제는 LOD 툴바에서만)

    TArray<FParticleSystemLOD> LODSettings; // LOD 레벨별 설정 (현재 편집 가능한 필드 없음)

    void SetSourcePath(const FString& InPath) { SourcePath = InPath; }
    const FString& GetSourcePath() const { return SourcePath; }
    const FString& GetAssetPathFileName() const override { return SourcePath; }

    // ── 전역 설정 접근자 ──────────────────────────────────────────
    float                     GetLODDistanceCheckTime() const { return LODDistanceCheckTime; }
    const TArray<FParticleSystemLOD>& GetLODSettings() const { return LODSettings; }
    EParticleSystemLODMethod  GetLODMethod() const { return LODMethod; }
    EParticleSystemUpdateMode GetSystemUpdateMode() const { return SystemUpdateMode; }
    float GetUpdateTimeFPS() const { return UpdateTimeFPS; }
    float GetWarmupTime() const { return WarmupTime; }
    float GetWarmupTickRate() const { return WarmupTickRate; }
    bool  GetUseFixedRelativeBoundingBox() const { return bUseFixedRelativeBoundingBox; }
    FVector GetFixedRelativeBoundsExtent() const { return FixedRelativeBoundsExtent; }
    float GetSecondsBeforeInactive() const { return SecondsBeforeInactive; }
    float GetDelay() const { return Delay; }
    float GetDelayLow() const { return DelayLow; }
    bool  GetUseDelayRange() const { return bUseDelayRange; }

  private:
    TArray<UParticleEmitter *> Emitters; // ParticleSystem 구성 Emitter 목록
    FString SourcePath;

    // ── LOD ───────────────────────────────────────────────────────
    UPROPERTY(Edit, Category="LOD", DisplayName="LOD Method", Type=Enum, Enum=StaticEnum_EParticleSystemLODMethod())
    EParticleSystemLODMethod LODMethod = EParticleSystemLODMethod::Automatic;

    // ── System 업데이트 방식 ──────────────────────────────────────
    UPROPERTY(Edit, Category="System", DisplayName="System Update Mode", Type=Enum, Enum=StaticEnum_EParticleSystemUpdateMode())
    EParticleSystemUpdateMode SystemUpdateMode = EParticleSystemUpdateMode::EPSUM_RealTime;

    UPROPERTY(Edit, Category="System", DisplayName="Update Time FPS", Min=1.0, Max=120.0, Speed=1.0)
    float UpdateTimeFPS = 60.0f;

    // ── Warmup ────────────────────────────────────────────────────
    UPROPERTY(Edit, Category="Warmup", DisplayName="Warmup Time", Min=0.0, Max=100.0, Speed=0.1)
    float WarmupTime = 0.0f;

    UPROPERTY(Edit, Category="Warmup", DisplayName="Warmup Tick Rate", Min=0.0, Max=100.0, Speed=0.1)
    float WarmupTickRate = 0.0f;

    // ── Bounds ────────────────────────────────────────────────────
    UPROPERTY(Edit, Category="Bounds", DisplayName="Use Fixed Bounds")
    bool bUseFixedRelativeBoundingBox = false;

    UPROPERTY(Edit, Category="Bounds", DisplayName="Fixed Bounds Extent")
    FVector FixedRelativeBoundsExtent = FVector(100.0f, 100.0f, 100.0f);

    // ── 활성 시간 ─────────────────────────────────────────────────
    UPROPERTY(Edit, Category="Activity", DisplayName="Seconds Before Inactive", Min=0.0, Max=60.0, Speed=0.1)
    float SecondsBeforeInactive = 0.0f;

    // ── 재생 지연 ─────────────────────────────────────────────────
    UPROPERTY(Edit, Category="Delay", DisplayName="Delay", Min=0.0, Max=100.0, Speed=0.1)
    float Delay = 0.0f;

    UPROPERTY(Edit, Category="Delay", DisplayName="Delay Low", Min=0.0, Max=100.0, Speed=0.1)
    float DelayLow = 0.0f;

    UPROPERTY(Edit, Category="Delay", DisplayName="Use Delay Range")
    bool bUseDelayRange = false;
};
