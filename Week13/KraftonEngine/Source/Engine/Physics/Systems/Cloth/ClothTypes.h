#pragma once

#include "Physics/Common/PhysicsDescTypes.h"

/** Cloth Simulation 방식 */
enum class EClothSimulationMode : uint8
{
    CSM_Verlet,     // Verlet Integration 기반 간단 Cloth
    CSM_PositionBD, // Position Based Dynamics 기반 Cloth
    CSM_External    // 외부 Cloth Solver 사용
};

/** Cloth Constraint 종류 */
enum class EClothConstraintType : uint8
{
    CCT_Distance, // 두 Particle 사이 거리 유지
    CCT_Bending,  // 접힘 저항
    CCT_Pin       // 특정 Particle 고정
};

/** Cloth Particle 초기 설정 */
struct FClothParticleDesc
{
    FVector LocalPosition = FVector::ZeroVector;
    float   InvMass       = 1.0f;  // 역질량. 0이면 고정 Particle
    bool    bPinned       = false;
};

/** Cloth Particle 사이 Constraint 설정 */
struct FClothConstraintDesc
{
    int32              ParticleA      = INDEX_NONE;
    int32              ParticleB      = INDEX_NONE;
    EClothConstraintType ConstraintType = EClothConstraintType::CCT_Distance;
    float              RestLength     = 0.0f;
    float              Stiffness      = 1.0f;
};
