// VehicleFilterConstants.h
#pragma once
#include "PxPhysicsAPI.h"

// Scene Query 필터 (레이캐스트용)
static const physx::PxU32 DRIVABLE_SURFACE = 0xffff0000;
static const physx::PxU32 UNDRIVABLE_SURFACE = 0x0000ffff;

// Simulation 필터 (충돌용)
static const physx::PxU32 COLLISION_FLAG_CHASSIS = (1 << 0);
static const physx::PxU32 COLLISION_FLAG_WHEEL = (1 << 1);
static const physx::PxU32 COLLISION_FLAG_GROUND = (1 << 2);

static const physx::PxU32 COLLISION_FLAG_CHASSIS_AGAINST = (1 << 2); // ground랑 충돌
static const physx::PxU32 COLLISION_FLAG_WHEEL_AGAINST = 0;        // 아무것도 충돌 안함
static const physx::PxU32 COLLISION_FLAG_GROUND_AGAINST = (1 << 0); // chassis랑 충돌
