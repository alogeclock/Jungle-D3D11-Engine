#pragma once

#include "Physics/Common/PhysicsDescTypes.h"

/** Physics Scene 종류 */
enum class EPhysicsSceneType : uint8
{
    PST_Game,    // 실제 게임 월드용 Physics Scene
    PST_Editor,  // Editor Preview용 Physics Scene
    PST_Preview  // Asset / Component 미리보기용 Physics Scene
};

/** Runtime Physics Actor 상태 */
enum class EPhysicsActorState : uint8
{
    PAS_None,      // 생성되지 않은 상태
    PAS_Created,   // Native Actor가 생성된 상태
    PAS_Added,     // Physics Scene에 등록된 상태
    PAS_Removed,   // Physics Scene에서 제거된 상태
    PAS_Destroyed  // Native Actor가 해제된 상태
};

/** Native Physics Actor 참조 핸들 */
struct FPhysicsActorHandle
{
    void* NativeActor = nullptr;

    bool IsValid() const { return NativeActor != nullptr; }
    void Reset()         { NativeActor = nullptr; }
};

/** Native Physics Shape 참조 핸들 */
struct FPhysicsShapeHandle
{
    void* NativeShape = nullptr;

    bool IsValid() const { return NativeShape != nullptr; }
    void Reset()         { NativeShape = nullptr; }
};

/** Native Physics Joint 참조 핸들 */
struct FPhysicsJointHandle
{
    void* NativeJoint = nullptr;

    bool IsValid() const { return NativeJoint != nullptr; }
    void Reset()         { NativeJoint = nullptr; }
};

/** Physics Step 실행 정보 */
struct FPhysicsStepInfo
{
    float DeltaTime    = 0.0f;
    int32 SubstepCount = 1;
    bool  bFetchResults = true;
};

/** Physics Runtime 통계 정보 */
struct FPhysicsRuntimeStats
{
    int32 BodyCount       = 0;
    int32 DynamicBodyCount = 0;
    int32 ActiveBodyCount = 0;
    int32 SleepingBodyCount = 0;
    int32 ConstraintCount = 0;
    int32 ContactCount    = 0;
    int32 ContactPairCount = 0;
    int32 ContactPointCount = 0;
    int32 ActiveRagdollCount = 0;
    int32 ActivePerBodyRagdollCount = 0;
    int32 ActiveAggregateRagdollCount = 0;
    int32 RagdollBodyCount = 0;
    int32 RagdollConstraintCount = 0;
    int32 AggregateCount = 0;
    int32 AggregateActorCount = 0;
    int32 PerBodyActorCount = 0;
    int32 SolverPositionIterationCount = 0;
    int32 SolverVelocityIterationCount = 0;
    int32 SolverConstraintCount = 0;
    int32 SolverContactCount = 0;
    float PreSimTimeMs = 0.0f;
    float SimulateTimeMs = 0.0f;
    float FetchResultsTimeMs = 0.0f;
    float PostSyncTimeMs = 0.0f;
    float TotalPhysicsTimeMs = 0.0f;
    float StepTimeMs      = 0.0f;
    float SyncTimeMs      = 0.0f;
};
