#pragma once

#include "Core/Singleton.h"
#include "Core/Log.h"

#include <NvCloth/Callbacks.h>

namespace nv
{
	namespace cloth
	{
		class Factory;
	}
}

class FNvClothAssertHandler : public physx::PxAssertHandler
{
public:
    void operator()(const char* Exp, const char* File, int Line, bool& Ignore) override
    {
        UE_LOG("[NvCloth] Assert: %s, %s, line %d", Exp, File, Line);
        Ignore = false;
    }
};

// Library-level NvCloth owner. Scene-local Solver objects are owned by FNvClothScene.
class FNvClothSDK : public TSingleton<FNvClothSDK>
{
    friend class TSingleton<FNvClothSDK>;

public:
    bool Initialize();
    void Shutdown();

    bool IsInitialized() const { return Factory != nullptr; }
    nv::cloth::Factory* GetFactory() const { return Factory; }

private:
    FNvClothAssertHandler AssertHandler;
    nv::cloth::Factory* Factory = nullptr;
};
