#pragma once

#include "ClothTypes.h"

class USkeletalMesh;

/** Cloth Simulation 설정 */
struct FClothSimulationSettings
{
    EClothSimulationMode SimulationMode      = EClothSimulationMode::CSM_Verlet;
    FVector              Gravity             = FVector(0.0f, -980.0f, 0.0f);
    int32                IterationCount      = 4;
    float                Damping             = 0.01f;
    float                Thickness           = 1.0f;
    float                WindScale           = 0.0f;
    bool                 bEnableSelfCollision = false;
};

/** Cloth Runtime 생성 정보 */
struct FClothBuildDesc
{
    USkeletalMesh*               SourceMesh = nullptr;
    TArray<FClothParticleDesc>   ParticleDescs;
    TArray<FClothConstraintDesc> ConstraintDescs;
    FClothSimulationSettings     SimulationSettings;

    bool IsValid() const
    {
        return SourceMesh != nullptr && !ParticleDescs.empty();
    }
};

/** Cloth Runtime 통계 정보 */
struct FClothRuntimeStats
{
    int32 ParticleCount   = 0;
    int32 ConstraintCount = 0;
    float SimulateTimeMs      = 0.0f;
    float ConstraintTimeMs    = 0.0f;
    float SkinningSyncTimeMs  = 0.0f;
};
