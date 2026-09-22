#pragma once

namespace physx
{
    class PxAllocatorCallback;
    class PxErrorCallback;
}

physx::PxAllocatorCallback& GetSharedPhysXAllocator();
physx::PxErrorCallback& GetSharedPhysXErrorCallback();
