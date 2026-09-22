#pragma once

#include "CoreTypes.h"
#include "Source/Engine/Public/Rendering/RenderProxy.h"

class FScene
{
public:
    FScene() = default;
    ~FScene() { Clear(); }

    void AddProxy(FRenderProxy* Proxy)
    {
        if (Proxy) Proxies.push_back(Proxy);
    }

    void RemoveProxy(FRenderProxy* Proxy)
    {
        auto it = std::find(Proxies.begin(), Proxies.end(), Proxy);
        if (it != Proxies.end()) Proxies.erase(it);
    }

    void Clear() { Proxies.clear(); }
    const TArray<FRenderProxy*>& GetProxies() const { return Proxies; }

private:
    TArray<FRenderProxy*> Proxies;
};