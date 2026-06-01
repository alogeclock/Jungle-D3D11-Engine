// VehicleFilterConstants.h
#pragma once
#include "PxPhysicsAPI.h"

// Vehicle suspension raycast용 query filter flag.
// Query filter layout:
// word0 = ECollisionChannel, word1 = block response bitmask, word2 = owner UUID, word3 = drivable flag.
// Simulation filter는 기존 엔진 layout(word0 channel, word1 block mask, word2 overlap mask, word3 owner UUID)을 유지한다.
static const physx::PxU32 DRIVABLE_SURFACE = (1 << 0);
static const physx::PxU32 UNDRIVABLE_SURFACE = 0;

// Simulation filter는 엔진의 ECollisionChannel/ResponseContainer 기반 필터를 그대로 사용한다.
