#pragma once

#include "CoreTypes.h"
#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"
#include <wrl.h>

using namespace Microsoft::WRL;

struct FTextMeshCache
{
    FString FontPath;
    FString Text;
    TArray<FTextureVertex> LocalVertices;
    TArray<uint32> LocalIndices;
};

struct FTextBatchBucket
{
    FString FontPath;
    TArray<FTextureVertex> BatchedVertices;
    TArray<uint32> BatchedIndices;
    
    FTextGpuBuffer GpuBuffer;
};

class UTextBatcherComponent : public UPrimitiveComponent
{
public:
    DECLARE_OBJECT(UTextBatcherComponent, UPrimitiveComponent)
    
    UTextBatcherComponent(const FString &InString);
    virtual ~UTextBatcherComponent() override;
public:
    void Submit(
        const FString& InFontPath,
        const FString& InText,
        const FMatrix<float>& InWorldMatrix);

    void SubmitSubUV(
        const FString& InFontPath,
        const FMatrix<float>& InWorldMatrix);

    virtual void Render(URenderer& renderer) override;
    void Flush(URenderer& renderer);

private:
    FTextBatchBucket* FindOrAddBucket(const FString& InFontPath);

private:
    TMap<FString, FTextMeshCache> MeshCaches;
    TArray<FTextBatchBucket> Buckets;
};