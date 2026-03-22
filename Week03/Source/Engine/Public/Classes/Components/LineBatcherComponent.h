#pragma once

#include <span>

#include "CoreTypes.h"
#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

struct FBatchedLine
{
    FVector<float> Start;
    FVector<float> End;
    FVector4<float> Color;

    FBatchedLine() : Start(), End(), Color() {};
    FBatchedLine(const FVector<float> &InStart, const FVector<float> &InEnd, const FVector4<float> &InColor) : Start(InStart), End(InEnd), Color(InColor) {}
};

struct FBatchedBox
{
    FBox Box;
    FVector4<float> Color;

    FBatchedBox() : Box(), Color() {};
    FBatchedBox(const FBox &InBox, const FVector4<float> &InColor) : Box(InBox), Color(InColor) {}
};

class ULineBatcherComponent : public UPrimitiveComponent
{
public:
    DECLARE_OBJECT(ULineBatcherComponent, UPrimitiveComponent)

    ULineBatcherComponent(const FString &InString);
    virtual ~ULineBatcherComponent() override;

private:
    TArray<FVertex> RenderVertices;
    TArray<uint16>  RenderIndices;
    ID3D11Buffer *DynamicVertexBuffer = nullptr;
    ID3D11Buffer *DynamicIndexBuffer = nullptr;
    uint32 VertexBufferSize = 0;
    uint32 IndexBufferSize = 0;

public:
    void DrawLine(const FVector<float> &Start, const FVector<float> &End, const FVector4<float> &Color);
    void DrawBox(const FBox &Box, const FVector4<float> Color);

    void Render(URenderer &renderer) override;
    void Flush();
};
