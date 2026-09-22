#include "Physics/Backends/PhysXSceneThreading.h"

#include <algorithm>
#include <thread>

// 하드웨어 논리 코어 수를 기반으로 PhysX dispatcher worker 수를 결정한다.
uint32 GetRecommendedPhysXWorkerThreadCount()
{
    const unsigned int LogicalCores = std::thread::hardware_concurrency();
    return static_cast<uint32>((std::min)(4u, (std::max)(1u, LogicalCores)));
}

// PhysX scene이 멀티스레드 solver, active actor, CCD 경로를 사용할 수 있도록 기본 flag를 설정한다.
void ApplyPhysXMultithreadedSceneSettings(physx::PxSceneDesc& SceneDesc)
{
    SceneDesc.flags |= physx::PxSceneFlag::eENABLE_PCM;
    SceneDesc.flags |= physx::PxSceneFlag::eENABLE_STABILIZATION;
    SceneDesc.flags |= physx::PxSceneFlag::eENABLE_ACTIVE_ACTORS;
    SceneDesc.flags |= physx::PxSceneFlag::eENABLE_CCD;
}
