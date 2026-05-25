/**
 * @file ParticleVertexTypes.h
 *  @brief Particle 렌더링용 Vertex / Instance 타입 정의.
 *
 * 포함 타입:
 * - FSpriteParticleInstanceVertex: Sprite Particle GPU 인스턴스 데이터
 * - FMeshParticleInstanceVertex:   Mesh   Particle GPU 인스턴스 데이터
 */

#pragma once

#include "../Runtime/ParticleRuntimeTypes.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"

/** Sprite Particle 인스턴싱용 per-instance 데이터
 *  - 하나의 Quad Vertex Buffer를 공유하고,
 *  - 인스턴스마다 위치/크기/회전/색상만 다르게 적용한다.
 */
struct FSpriteParticleInstanceVertex
{
    FVector  Position;         // world-space position
    float    Rotation;         // billboard rotation in radians
    FVector4 Size;             // xy: width/height, zw: unused
    FVector4 Color;            // rgba: normalized [0,1]
};

/** Mesh Particle 인스턴싱용 per-instance 데이터
 *  - 하나의 Static/Skeletal Mesh를 공유하고,
 *  - 인스턴스마다 Transform/Color만 다르게 적용한다.
 */
struct FMeshParticleInstanceVertex
{
    FMatrix  Transform; // local-to-world transform, scale 포함
    FVector4 Color;     // rgba: normalized [0,1]
};
