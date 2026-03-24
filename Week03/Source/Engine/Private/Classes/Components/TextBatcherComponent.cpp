#include "Source/Engine/Public/Classes/Components/TextBatcherComponent.h"
#include "Source/Engine/Public/Classes/TextMeshBuilder.h"
#include <iostream>

UTextBatcherComponent::UTextBatcherComponent(const FString& InString) : UPrimitiveComponent(InString)
{
    PrimitiveType = EPrimitiveType::TextBatcher;
    CullMode = ECullMode::None;
    bEnableDepthTest = false;
}

UTextBatcherComponent::~UTextBatcherComponent()
{
}


void UTextBatcherComponent::Submit(const FString& InFontPath, const FString& InText, const FMatrix<float>& InWorldMatrix)
{
    if (InText.empty()) return;
    
    const FString CacheKey = InFontPath + "\x1F" + InText;

    auto It = MeshCaches.find(CacheKey);
    if (It == MeshCaches.end())
    {
        FTextMeshCache NewCache;
        NewCache.FontPath = InFontPath;
        NewCache.Text = InText;
        FTextMeshBuilder::BuildTextMesh(InText, &NewCache.LocalVertices, &NewCache.LocalIndices);

        It = MeshCaches.emplace(CacheKey, std::move(NewCache)).first;
    }
    
    const FTextMeshCache & Cache = (It->second);
    if (Cache.LocalVertices.empty()||Cache.LocalIndices.empty())
    {
        return;
    }

    FTextBatchBucket* Bucket = FindOrAddBucket(InFontPath);
    if (Bucket == nullptr) return;
        
    const uint32 BaseIndex = static_cast<uint32>(Bucket->BatchedVertices.size());
    
    Bucket->BatchedVertices.reserve(
        Bucket->BatchedVertices.size() + Cache.LocalVertices.size());
    Bucket->BatchedIndices.reserve(    // ← 수정
        Bucket->BatchedIndices.size() + Cache.LocalIndices.size());
    
    for (const FTextureVertex& Vertex : Cache.LocalVertices)
    {
        const FVector4<float> LocalPos(
            Vertex.Position.X,
            Vertex.Position.Y,
            Vertex.Position.Z,
            1.0f);

        const FVector4<float> WorldPos = LocalPos * InWorldMatrix;

        Bucket->BatchedVertices.push_back({
            FVector<float>(WorldPos.X, WorldPos.Y, WorldPos.Z),
            Vertex.u,
            Vertex.v
        });
    }

    for (uint32 Index :Cache.LocalIndices)
    {
        Bucket->BatchedIndices.push_back(BaseIndex + Index);
    }
}

void UTextBatcherComponent::Render(URenderer& renderer)
{
    if (!IsVisible())
    {
        return;
    }

    if (Buckets.size() == 0)
        return;

    renderer.SetDepthStencilEnable(bEnableDepthTest);
    renderer.SetCullMode(CullMode);

    FConstants Constants = {};
    Constants.MVPMatrix = FMatrix<float>::Identity();

    for (FTextBatchBucket& Bucket : Buckets)
    {
        if (Bucket.BatchedVertices.empty() || Bucket.BatchedIndices.empty())
        {
            continue;
        }

        renderer.RenderTextBatch(
            Bucket.FontPath,
            Constants,
            Bucket.BatchedVertices,
            Bucket.BatchedIndices,
            Bucket.GpuBuffer
        );
    }
}

void UTextBatcherComponent::Flush(URenderer& renderer)
{
    renderer.UndoRenderText();

    for (FTextBatchBucket& Bucket : Buckets)
    {
        Bucket.BatchedVertices.clear();
        Bucket.BatchedIndices.clear();
    }
}

FTextBatchBucket* UTextBatcherComponent::FindOrAddBucket(const FString& InFontPath)
{
   for (FTextBatchBucket& Bucket : Buckets)
    {
        if (Bucket.FontPath == InFontPath)
        {
            return &Bucket;
        }
    }

    FTextBatchBucket NewBucket;
    NewBucket.FontPath = InFontPath;
    Buckets.push_back(std::move(NewBucket));
    return &Buckets.back();
}

