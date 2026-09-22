#include "Render/Proxy/ClothSceneProxy.h"

#include "Component/ClothComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Physics/Systems/Cloth/ClothInstance.h"
#include "Profiling/Stats.h"
#include "Render/Command/DrawCommand.h"

FClothSceneProxy::FClothSceneProxy(UClothComponent* InComponent)
    : FPrimitiveSceneProxy(InComponent)
{
    RebuildSectionDraws();
}

FClothSceneProxy::~FClothSceneProxy()
{
    DynamicVertexBuffer.Release();
    DynamicIndexBuffer.Release();
}

UClothComponent* FClothSceneProxy::GetClothComponent() const
{
    return static_cast<UClothComponent*>(GetOwner());
}

void FClothSceneProxy::UpdateMaterial()
{
    RebuildSectionDraws();
}

void FClothSceneProxy::UpdateMesh()
{
    MeshBuffer = nullptr;
    UploadedRenderRevision = 0;
    UploadedIndexCount = 0;
    DynamicVertexBuffer.Release();
    DynamicIndexBuffer.Release();
    RebuildSectionDraws();
}

bool FClothSceneProxy::PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const
{
    SCOPE_STAT_CAT("BuildRenderData", "Cloth");

    UClothComponent* ClothComponent = GetClothComponent();
    if (!ClothComponent || !Device || !Context)
    {
        return false;
    }

    const FClothInstance& ClothInstance = ClothComponent->GetClothInstance();
    const TArray<FVertexPNCTT>& Vertices = ClothInstance.GetRenderVertices();
    const TArray<uint32>& Indices = ClothInstance.GetRenderIndices();
    if (Vertices.empty() || Indices.empty())
    {
        return false;
    }

    const uint32 VertexCount = static_cast<uint32>(Vertices.size());
    const uint32 IndexCount = static_cast<uint32>(Indices.size());
    if (!DynamicVertexBuffer.GetBuffer())
    {
        DynamicVertexBuffer.Create(Device, VertexCount, sizeof(FVertexPNCTT));
    }
    if (!DynamicIndexBuffer.GetBuffer())
    {
        DynamicIndexBuffer.Create(Device, IndexCount);
    }

    DynamicVertexBuffer.EnsureCapacity(Device, VertexCount);
    DynamicIndexBuffer.EnsureCapacity(Device, IndexCount);

    const uint64 CurrentRevision = ClothInstance.GetRenderRevision();
    if (UploadedRenderRevision != CurrentRevision)
    {
        if (!DynamicVertexBuffer.Update(Context, Vertices.data(), VertexCount))
        {
            return false;
        }
        UploadedRenderRevision = CurrentRevision;
    }

    if (UploadedIndexCount != IndexCount)
    {
        if (!DynamicIndexBuffer.Update(Context, Indices.data(), IndexCount))
        {
            return false;
        }
        UploadedIndexCount = IndexCount;
    }

    OutBuffer = {};
    OutBuffer.VB = DynamicVertexBuffer.GetBuffer();
    OutBuffer.VBStride = DynamicVertexBuffer.GetStride();
    OutBuffer.IB = DynamicIndexBuffer.GetBuffer();
    return OutBuffer.VB != nullptr && OutBuffer.IB != nullptr;
}

void FClothSceneProxy::RebuildSectionDraws()
{
    SectionDraws.clear();

    UClothComponent* ClothComponent = GetClothComponent();
    if (!ClothComponent)
    {
        return;
    }

    UMaterial* Material = ClothComponent->GetMaterial(0);
    if (!Material)
    {
        Material = FMaterialManager::Get().GetOrCreateMaterial("None");
    }

    const TArray<uint32>& Indices = ClothComponent->GetClothInstance().GetRenderIndices();
    if (!Indices.empty())
    {
        SectionDraws.push_back({ Material, 0, static_cast<uint32>(Indices.size()) });
    }
}
