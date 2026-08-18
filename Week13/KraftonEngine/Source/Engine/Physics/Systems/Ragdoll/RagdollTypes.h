#pragma once

#include "Physics/Common/PhysicsDescTypes.h"

/** Ragdoll 시뮬레이션 상태 */
enum class ERagdollSimulationState : uint8
{
    RSS_Disabled,      // Ragdoll 비활성화
    RSS_KinematicPose, // Animation Pose를 Physics Body에 동기화하는 상태
    RSS_Simulating     // Physics Simulation 결과를 Bone에 반영하는 상태
};

/** Animation Pose와 Physics Pose의 혼합 방식 */
enum class ERagdollBlendMode : uint8
{
    RBM_None,        // 혼합 없음
    RBM_PhysicsOnly, // Physics Pose만 사용
    RBM_BlendToAnim, // Physics Pose에서 Animation Pose로 복귀
    RBM_BlendToPhys  // Animation Pose에서 Physics Pose로 전환
};

/** Skeleton Bone과 Physics Body 설정 사이의 매핑 */
struct FRagdollBoneMapping
{
    FName BoneName  = FName::None;
    int32 BoneIndex = INDEX_NONE;
    int32 BodyIndex = INDEX_NONE;
};
