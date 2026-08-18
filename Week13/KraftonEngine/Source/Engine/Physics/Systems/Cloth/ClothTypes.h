#pragma once

#include "Object/ObjectMacros.h"
#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "ClothTypes.generated.h"

/** Cloth Simulation 방식 */
UENUM()
enum class EClothSimulationMode : uint8
{
    CSM_Verlet,
    CSM_PositionBD,
    CSM_External
};

/** Cloth Runtime 데이터의 입력 원본 */
UENUM()
enum class EClothSourceType : uint8
{
    Grid,
    StaticMesh,
    SkeletalMeshSection,
};

/** Grid Cloth에서 고정할 Particle 영역 */
UENUM()
enum class EClothPinMode : uint8
{
    None,
    TopRow,
    TopCorners,
    LeftEdge,
    Custom,
};

/** Cloth Constraint 종류 */
UENUM()
enum class EClothConstraintType : uint8
{
    CCT_Distance,
    CCT_Bending,
    CCT_Pin
};

/** Cloth Particle 초기 설정 */
USTRUCT()
struct FClothParticleDesc
{
    GENERATED_BODY(FClothParticleDesc)

    FVector LocalPosition = FVector::ZeroVector;
    float InvMass = 1.0f;
    bool bPinned = false;
    float MaxDistance = -1.0f;
};

/** Cloth Particle 사이 Constraint 설정 */
USTRUCT()
struct FClothConstraintDesc
{
    GENERATED_BODY(FClothConstraintDesc)

    int32 ParticleA = INDEX_NONE;
    int32 ParticleB = INDEX_NONE;
    EClothConstraintType ConstraintType = EClothConstraintType::CCT_Distance;
    float RestLength = 0.0f;
    float Stiffness = 1.0f;
};

/** 독립 Plane/Grid Cloth 생성 정보. 기본값만으로 월드에 천이 보이도록 설정한다. */
USTRUCT()
struct FClothGridDesc
{
    GENERATED_BODY(FClothGridDesc)

    UPROPERTY(Category = "Cloth", DisplayName = "Width", Min = 2, Max = 128, Speed = 1.0)
    int32 Width = 16;

    UPROPERTY(Category = "Cloth", DisplayName = "Height", Min = 2, Max = 128, Speed = 1.0)
    int32 Height = 12;

    UPROPERTY(Category = "Cloth", DisplayName = "Spacing", Min = 1.0, Max = 100.0, Speed = 0.5)
    float Spacing = 1.0f;

    FVector InitialOffset = FVector(0.0f, 0.0f, 0.0f);

    UPROPERTY(Category = "Cloth", DisplayName = "Pin Mode", Type = Enum, Enum = StaticEnum_EClothPinMode())
    EClothPinMode PinMode = EClothPinMode::TopRow;
};

/** Constraint phase별 stiffness 설정 */
USTRUCT()
struct FClothConstraintSettings
{
    GENERATED_BODY(FClothConstraintSettings)

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Use Shear")
    bool bUseShear = true;

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Use Bending")
    bool bUseBending = true;

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Use Tether")
    bool bUseTether = true;

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Structural Stiffness", Min = 0.0, Max = 1.0, Speed = 0.01)
    float StructuralStiffness = 1.0f;

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Shear Stiffness", Min = 0.0, Max = 1.0, Speed = 0.01)
    float ShearStiffness = 0.8f;

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Bending Stiffness", Min = 0.0, Max = 1.0, Speed = 0.01)
    float BendingStiffness = 0.35f;

    UPROPERTY(Edit, Category = "Cloth", DisplayName = "Tether Stiffness", Min = 0.0, Max = 1.0, Speed = 0.01)
    float TetherStiffness = 1.0f;
};
