#include "Source/Engine/Public/Classes/Components/LineBatcherComponent.h"

ULineBatcherComponent::ULineBatcherComponent(const FString &InString) : UPrimitiveComponent(InString) { PrimitiveType = EPrimitiveType::LineBatcher; }

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
    const uint16 StartIndex = static_cast<uint16>(RenderVertices.size());

    RenderVertices.emplace_back(Start, Color);
    RenderVertices.emplace_back(End, Color);

    RenderIndices.emplace_back(StartIndex);
    RenderIndices.emplace_back(StartIndex + 1);
}

void ULineBatcherComponent::DrawLines(std::span<const FBatchedLine> Lines)
{
    if (Lines.empty())
        return;

    const uint16 StartIndex = static_cast<uint16>(RenderVertices.size());
    const size_t LineCount = Lines.size();

    // 임시 벡터에 모든 데이터 미리 구성
    TArray<FVertex> TempVertices;
    TArray<uint16> TempIndices;

    TempVertices.reserve(LineCount * 2);
    TempIndices.reserve(LineCount * 2);

    for (size_t i = 0; i < LineCount; ++i)
    {
        const auto &Line = Lines[i];
        TempVertices.emplace_back(Line.Start, Line.Color);
        TempVertices.emplace_back(Line.End, Line.Color);

        const uint16 Index = static_cast<uint16>(StartIndex + i * 2);
        TempIndices.emplace_back(Index);
        TempIndices.emplace_back(Index + 1);
    }

    // 한 번에 삽입
    RenderVertices.insert(RenderVertices.end(), TempVertices.begin(), TempVertices.end());
    RenderIndices.insert(RenderIndices.end(), TempIndices.begin(), TempIndices.end());
}

void ULineBatcherComponent::DrawBox(const FBox &Box, FVector4<float> Color)
{
    const uint16 StartIndex = static_cast<uint16>(RenderVertices.size());

    const FVector<float> Min = Box.Min;
    const FVector<float> Max = Box.Max;

    RenderVertices.emplace_back(FVector<float>(Min.X, Min.Y, Min.Z), Color);
    RenderVertices.emplace_back(FVector<float>(Max.X, Min.Y, Min.Z), Color);
    RenderVertices.emplace_back(FVector<float>(Max.X, Max.Y, Min.Z), Color);
    RenderVertices.emplace_back(FVector<float>(Min.X, Max.Y, Min.Z), Color);
    RenderVertices.emplace_back(FVector<float>(Min.X, Min.Y, Max.Z), Color);
    RenderVertices.emplace_back(FVector<float>(Max.X, Min.Y, Max.Z), Color);
    RenderVertices.emplace_back(FVector<float>(Max.X, Max.Y, Max.Z), Color);
    RenderVertices.emplace_back(FVector<float>(Min.X, Max.Y, Max.Z), Color);

    constexpr uint16 LocalIndices[] = {
        0, 1, 1, 2, 2, 3, 3, 0, // 앞면
        4, 5, 5, 6, 6, 7, 7, 4, // 뒷면
        0, 4, 1, 5, 2, 6, 3, 7  // 연결
    };

    for (uint16 Index : LocalIndices)
    {
        RenderIndices.emplace_back(StartIndex + Index);
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

        VertexBufferSize = (std::max)(RequiredVertexBufferSize, VertexBufferSize * 2);

        DynamicVertexBuffer = renderer.CreateDynamicVertexBuffer(VertexBufferSize);
    }

    if (RequiredIndexBufferSize > IndexBufferSize)
    {
        if (DynamicIndexBuffer)
        {
            DynamicIndexBuffer->Release();
        }
        IndexBufferSize = (std::max)(RequiredIndexBufferSize, IndexBufferSize * 2);
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
    RenderVertices.clear();
    RenderIndices.clear();
}
