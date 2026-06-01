#pragma once

#include "Core/CoreTypes.h"

#include <PxPhysicsAPI.h>

// PhysX scene read lock wrapper.
// PxSceneReadLock 대신 nullptr 방어와 파일/라인 전달을 한 곳에 모은다.
class FPhysXSceneReadLock
{
public:
    FPhysXSceneReadLock(physx::PxScene* InScene, const char* FileName, physx::PxU32 LineNumber)
        : Scene(InScene)
    {
        if (Scene)
        {
            Scene->lockRead(FileName, LineNumber);
        }
    }

    ~FPhysXSceneReadLock()
    {
        if (Scene)
        {
            Scene->unlockRead();
        }
    }

    FPhysXSceneReadLock(const FPhysXSceneReadLock&) = delete;
    FPhysXSceneReadLock& operator=(const FPhysXSceneReadLock&) = delete;

private:
    physx::PxScene* Scene = nullptr;
};

// PhysX scene write lock wrapper.
// Scene 구조 변경과 actor 쓰기 API 호출 구간을 명시적으로 보호한다.
class FPhysXSceneWriteLock
{
public:
    FPhysXSceneWriteLock(physx::PxScene* InScene, const char* FileName, physx::PxU32 LineNumber)
        : Scene(InScene)
    {
        if (Scene)
        {
            Scene->lockWrite(FileName, LineNumber);
        }
    }

    ~FPhysXSceneWriteLock()
    {
        if (Scene)
        {
            Scene->unlockWrite();
        }
    }

    FPhysXSceneWriteLock(const FPhysXSceneWriteLock&) = delete;
    FPhysXSceneWriteLock& operator=(const FPhysXSceneWriteLock&) = delete;

private:
    physx::PxScene* Scene = nullptr;
};

#define PHYSX_SCENE_JOIN_INNER(X, Y) X##Y
#define PHYSX_SCENE_JOIN(X, Y) PHYSX_SCENE_JOIN_INNER(X, Y)

#define SCOPED_PHYSX_SCENE_READ_LOCK(Scene) FPhysXSceneReadLock PHYSX_SCENE_JOIN(PhysXSceneReadLock, __LINE__)(Scene, __FILE__, __LINE__)
#define SCOPED_PHYSX_SCENE_WRITE_LOCK(Scene) FPhysXSceneWriteLock PHYSX_SCENE_JOIN(PhysXSceneWriteLock, __LINE__)(Scene, __FILE__, __LINE__)

uint32 GetRecommendedPhysXWorkerThreadCount();
void ApplyPhysXMultithreadedSceneSettings(physx::PxSceneDesc& SceneDesc);
