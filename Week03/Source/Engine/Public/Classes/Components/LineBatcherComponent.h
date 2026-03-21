#pragma once

#include <span>

#include "CoreTypes.h"
#include "Source/Engine/Public/Classes/Components/PrimitiveComponent.h"

struct FBatchedLine
{
    FVector<float> Start;
    FVector<float> End;
    //FLinearColor Color;
    FVector4<float> Color;
    //float Thickness;

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
    TArray<FBatchedLine> BatchedLines;
    TArray<FBatchedBox>  BatchedBoxes;
    TArray<FVertex> RenderVertices;
    TArray<uint16>  RenderIndices;
    ID3D11Buffer *DynamicVertexBuffer = nullptr;
    ID3D11Buffer *DynamicIndexBuffer = nullptr;
    uint32 VertexBufferSize = 0;
    uint32 IndexBufferSize = 0;

public:
    void DrawLine(const FVector<float> &Start, const FVector<float> &End, const FVector4<float> &Color);
    void DrawLines(std::span<const FBatchedLine> InLines);
    //void DrawBox(const FBox &Box, const FMatrix<float> &TM, FVector4<float> Color);
    //void DrawBox(const FVector<float> &Center, const FVector<float> &Box, FVector4<float> Color);
    void DrawBox(const FBox &Box, const FVector4<float> Color);

    // 렌더링 직전에 RenderVertices와 RenderIndices를 비우고 업데이트
    void Build();
    void Render(URenderer &renderer) override;
    void Flush();
};
