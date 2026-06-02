#include "Physics/Backends/NvClothSDK.h"
#include "Physics/Backends/PhysXSharedCallbacks.h"

#include <NvCloth/Callbacks.h>
#include <NvCloth/Factory.h>

bool FNvClothSDK::Initialize()
{
    if (Factory)
    {
        return true;
    }

    nv::cloth::InitializeNvCloth(&GetSharedPhysXAllocator(), &GetSharedPhysXErrorCallback(), &AssertHandler, nullptr);

    Factory = NvClothCreateFactoryCPU();
    if (!Factory)
    {
        UE_LOG("[NvCloth] Failed to create CPU Factory");
        return false;
    }

    UE_LOG("[NvCloth] SDK initialized with CPU Factory");
    return true;
}

void FNvClothSDK::Shutdown()
{
    if (Factory)
    {
        NvClothDestroyFactory(Factory);
        Factory = nullptr;
    }

    UE_LOG("[NvCloth] SDK shutdown");
}
