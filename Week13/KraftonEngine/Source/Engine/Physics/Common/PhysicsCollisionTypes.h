#pragma once

#include "PhysicsTypes.h"

/** Collision 사용 방식 */
enum class EPhysicsCollisionEnabled : uint8
{
    PCE_NoCollision,     // Query / Physics 모두 사용하지 않음
    PCE_QueryOnly,       // Raycast / Sweep / Overlap Query만 사용
    PCE_PhysicsOnly,     // 물리 시뮬레이션 충돌만 사용
    PCE_QueryAndPhysics  // Query와 Physics 충돌 모두 사용
};

/** Collision 응답 타입 */
enum class EPhysicsCollisionResponse : uint8
{
    PCR_Ignore,  // 충돌 무시
    PCR_Overlap, // 겹침 이벤트만 발생
    PCR_Block    // 충돌을 막고 Hit 처리
};

/** Collision 채널 */
enum class EPhysicsCollisionChannel : uint8
{
    PCC_WorldStatic,  // 움직이지 않는 월드 지형 / 오브젝트
    PCC_WorldDynamic, // 움직이는 월드 오브젝트
    PCC_Pawn,         // 캐릭터 / Pawn
    PCC_PhysicsBody,  // 물리 Body
    PCC_Visibility,   // Visibility Trace
    PCC_Camera,       // Camera Trace
    PCC_Custom        // 확장용 채널
};

/** 채널별 Collision 응답 설정 */
struct FPhysicsCollisionResponsePair
{
    EPhysicsCollisionChannel  Channel  = EPhysicsCollisionChannel::PCC_WorldStatic;
    EPhysicsCollisionResponse Response = EPhysicsCollisionResponse::PCR_Block;
};

/** Body / Shape 공용 Collision 설정 */
struct FPhysicsCollisionDesc
{
    EPhysicsCollisionEnabled CollisionEnabled = EPhysicsCollisionEnabled::PCE_QueryAndPhysics;
    EPhysicsCollisionChannel ObjectChannel   = EPhysicsCollisionChannel::PCC_PhysicsBody;

    TArray<FPhysicsCollisionResponsePair> Responses;
};
