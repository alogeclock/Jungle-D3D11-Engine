#pragma once

#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Render/Resource/Buffer.h"

class UClothComponent;
struct FDrawCommandBuffer;

class FClothSceneProxy : public FPrimitiveSceneProxy
{
public:
    explicit FClothSceneProxy(UClothComponent* InComponent);
    ~FClothSceneProxy() override;

    void UpdateMaterial() override;
    void UpdateMesh() override;
    bool ShouldOverrideCullMode() const override { return true; }
    ERasterizerState GetCullModeOverride() const override { return ERasterizerState::SolidNoCull; }
    bool PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const override;

private:
    void RebuildSectionDraws();
    UClothComponent* GetClothComponent() const;

private:
    mutable FDynamicVertexBuffer DynamicVertexBuffer;
    mutable FDynamicIndexBuffer DynamicIndexBuffer;
    mutable uint64 UploadedRenderRevision = 0;
    mutable uint32 UploadedIndexCount = 0;
};
