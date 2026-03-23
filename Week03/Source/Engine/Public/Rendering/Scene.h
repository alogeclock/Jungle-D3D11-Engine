#pragma once

#include "CoreTypes.h"
#include "Source/Engine/Public/Rendering/RenderProxy.h"

class FScene
{
public:
    FScene() {}
    ~FScene() { Clear(); }

    void AddRenderProxy(FRenderProxy* Proxy)
    {
        if (Proxy)
        {
            RenderProxies.push_back(Proxy);
        }
    }

    void Clear()
    {
        for (FRenderProxy* Proxy : RenderProxies)
        {
            delete Proxy; // 매 프레임 동적 할당된 프록시 정리
        }
        RenderProxies.clear();
    }

    const TArray<FRenderProxy*>& GetRenderProxies() const { return RenderProxies; }

private:
    TArray<FRenderProxy*> RenderProxies;
};