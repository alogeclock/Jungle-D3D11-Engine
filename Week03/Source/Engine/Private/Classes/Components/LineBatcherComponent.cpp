#include "Source/Engine/Public/Classes/Components/LineBatcherComponent.h"

namespace
{
    constexpr uint32 INITIAL_VERTEX_CAPACITY = 1000 * sizeof(FVertex);
    constexpr uint32 INITIAL_INDEX_CAPACITY = 1000 * sizeof(uint16);
}

ULineBatcherComponent::ULineBatcherComponent(const FString &InString) : UPrimitiveComponent(InString) 
{ 
    PrimitiveType = EPrimitiveType::LineBatcher;
}

ULineBatcherComponent::~ULineBatcherComponent() 
{
    if (DynamicVertexBuffer)
    {
        DynamicVertexBuffer->Release();
        DynamicVertexBuffer = nullptr;
    }
    if (DynamicIndexBuffer)
    {
        DynamicIndexBuffer->Release();
        DynamicIndexBuffer = nullptr;
    }
}

void ULineBatcherComponent::DrawLine(const FVector<float> &Start, const FVector<float> &End, const FVector4<float> &Color)
{
    BatchedLines.emplace_back(Start, End, Color); // TArray.Emplace()
}

void ULineBatcherComponent::DrawLines(std::span<const FBatchedLine> InLines) { BatchedLines.insert(BatchedLines.end(), InLines.begin(), InLines.end()); }

//void ULineBatcherComponent::DrawBox(const FBox &Box, const FMatrix<float> &TM, FVector4<float> Color) {}

void ULineBatcherComponent::DrawBox(const FBox &Box, FVector4<float> Color) 
{
    BatchedBoxes.emplace_back(Box, Color);
}

void ULineBatcherComponent::Build()
{
    RenderVertices.clear();
    RenderIndices.clear();
    const uint32 TotalVertices = static_cast<uint32>(BatchedLines.size() * 2 + BatchedBoxes.size() * 8);
    const uint32 TotalIndices = static_cast<uint32>(BatchedLines.size() * 2 + BatchedBoxes.size() * 24);
    RenderVertices.reserve(TotalVertices);
    RenderIndices.reserve(TotalIndices);

    for (const FBatchedLine &Line : BatchedLines)
    {
        const uint16 StartIndex = static_cast<uint16>(RenderVertices.size());
        RenderVertices.emplace_back(Line.Start, Line.Color);
        RenderVertices.emplace_back(Line.End, Line.Color);
        RenderIndices.emplace_back(StartIndex);
        RenderIndices.emplace_back(StartIndex + 1);
    }

    for (const FBatchedBox &Box : BatchedBoxes)
    {
        const FVector<float> Min = Box.Box.Min;
        const FVector<float> Max = Box.Box.Max;
        const uint16 StartIndex = static_cast<uint16>(RenderVertices.size());
        RenderVertices.emplace_back(FVector<float>(Min.X, Min.Y, Min.Z), Box.Color);
        RenderVertices.emplace_back(FVector<float>(Max.X, Min.Y, Min.Z), Box.Color);
        RenderVertices.emplace_back(FVector<float>(Max.X, Max.Y, Min.Z), Box.Color);
        RenderVertices.emplace_back(FVector<float>(Min.X, Max.Y, Min.Z), Box.Color);
        RenderVertices.emplace_back(FVector<float>(Min.X, Min.Y, Max.Z), Box.Color);
        RenderVertices.emplace_back(FVector<float>(Max.X, Min.Y, Max.Z), Box.Color);
        RenderVertices.emplace_back(FVector<float>(Max.X, Max.Y, Max.Z), Box.Color);
        RenderVertices.emplace_back(FVector<float>(Min.X, Max.Y, Max.Z), Box.Color);

        constexpr uint16 Indices[] = {
            0, 1, 1, 2, 2, 3, 3, 0, // 앞면
            4, 5, 5, 6, 6, 7, 7, 4, // 뒷면
            0, 4, 1, 5, 2, 6, 3, 7  // 연결
        };

        for (uint16 Index : Indices)
        {
            RenderIndices.emplace_back(StartIndex + Index);
        }
    }
}

void ULineBatcherComponent::Render(URenderer &renderer) 
{ 
    if (RenderVertices.empty() || RenderIndices.empty())
    {
        return;
    }

    renderer.SetTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    uint32 RequiredVertexBufferSize = static_cast<uint32>(RenderVertices.size() * sizeof(FVertex));
    uint32 RequiredIndexBufferSize = static_cast<uint32>(RenderIndices.size() * sizeof(uint16));

    if (RequiredVertexBufferSize > VertexBufferSize)
    {
        if (DynamicVertexBuffer)
        {
            DynamicVertexBuffer->Release();
        }

        VertexBufferSize = (std::max)(RequiredVertexBufferSize, INITIAL_VERTEX_CAPACITY);
        DynamicVertexBuffer = renderer.CreateDynamicVertexBuffer(VertexBufferSize);
    }

    if (RequiredIndexBufferSize > IndexBufferSize)
    {
        if (DynamicIndexBuffer)
        {
            DynamicIndexBuffer->Release();
        }
        IndexBufferSize = (std::max)(RequiredIndexBufferSize, INITIAL_INDEX_CAPACITY);
        DynamicIndexBuffer = renderer.CreateDynamicIndexBuffer(IndexBufferSize);
    }

    renderer.UpdateDynamicBuffer(DynamicVertexBuffer, RenderVertices.data(), RequiredVertexBufferSize);
    renderer.UpdateDynamicBuffer(DynamicIndexBuffer, RenderIndices.data(), RequiredIndexBufferSize);
    FConstants constants = {};
    constants.MVPMatrix = FMatrix<float>::Identity();
    renderer.UpdateConstant(constants);

    renderer.DrawIndexed(DynamicVertexBuffer, DynamicIndexBuffer, static_cast<uint32>(RenderIndices.size()), sizeof(FVertex));
}

void ULineBatcherComponent::Flush()
{
    BatchedLines.clear();
    BatchedBoxes.clear();
}
