#pragma once

#include "Object/ObjectMacros.h"
#include "Physics/Systems/Cloth/ClothTypes.h"
#include "ClothSimulationTypes.generated.h"

class USkeletalMesh;

/** Cloth Simulation 설정 */
USTRUCT()
struct FClothSimulationSettings
{
    GENERATED_BODY(FClothSimulationSettings)

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Simulation Mode", Type = Enum, Enum = StaticEnum_EClothSimulationMode())
    EClothSimulationMode SimulationMode = EClothSimulationMode::CSM_External;

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Gravity")
    FVector Gravity = FVector(0.0f, 0.0f, -980.0f);

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Substep Count", Min = 1, Max = 8, Speed = 1.0)
    int32 SubstepCount = 1;

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Solver Frequency", Min = 1.0, Max = 1000.0, Speed = 1.0)
    float SolverFrequency = 120.0f;

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Stiffness Frequency", Min = 1.0, Max = 1000.0, Speed = 1.0)
    float StiffnessFrequency = 120.0f;

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Damping")
    FVector Damping = FVector(0.05f, 0.05f, 0.05f);

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Linear Drag")
    FVector LinearDrag = FVector(0.0f, 0.0f, 0.0f);

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Angular Drag")
    FVector AngularDrag = FVector(0.0f, 0.0f, 0.0f);

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Thickness", Min = 0.0, Speed = 0.1)
    float Thickness = 1.0f;

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Friction", Min = 0.0, Max = 1.0, Speed = 0.01)
    float Friction = 0.5f;

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Collision Mass Scale", Min = 0.0, Speed = 0.1)
    float CollisionMassScale = 1.0f;

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Enable CCD")
    bool bEnableCCD = false;

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Enable Self Collision")
    bool bEnableSelfCollision = false;

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Teleport Distance Threshold", Min = 0.0, Speed = 1.0)
    float TeleportDistanceThreshold = 100.0f;

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Wind Scale", Min = 0.0, Speed = 0.1)
    float WindScale = 0.0f;

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Render Normal Offset", Min = 0.0, Speed = 0.01)
    float RenderNormalOffset = 0.0f;
};

/** Cloth Runtime 생성 정보 */
USTRUCT()
struct FClothBuildDesc
{
    GENERATED_BODY(FClothBuildDesc)

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Source Type", Type = Enum, Enum = StaticEnum_EClothSourceType())
    EClothSourceType SourceType = EClothSourceType::Grid;

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Grid Desc")
    FClothGridDesc GridDesc;

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Simulation Settings")
    FClothSimulationSettings SimulationSettings;

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Constraint Settings")
    FClothConstraintSettings ConstraintSettings;

    TArray<FClothParticleDesc> ParticleDescs;
    TArray<FClothConstraintDesc> ConstraintDescs;
    USkeletalMesh* SourceMesh = nullptr;

    bool IsValid() const
    {
        if (SourceType == EClothSourceType::Grid)
        {
            return GridDesc.Width >= 2 && GridDesc.Height >= 2 && GridDesc.Spacing > 0.0f;
        }

        if (SourceType == EClothSourceType::StaticMesh)
        {
            return true;
        }

        return SourceMesh != nullptr && !ParticleDescs.empty();
    }
};

/** Cloth Runtime 통계 정보 */
struct FClothRuntimeStats
{
    int32 ParticleCount = 0;
    int32 ConstraintCount = 0;
    float SimulateTimeMs = 0.0f;
    float ConstraintTimeMs = 0.0f;
    float SkinningSyncTimeMs = 0.0f;
};
