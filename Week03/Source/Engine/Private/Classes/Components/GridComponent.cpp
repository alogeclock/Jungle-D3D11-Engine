#include "Source/Engine/Public/Classes/Components/GridComponent.h"
#include "Source/Engine/Public/Classes/Components/LineBatcherComponent.h"
#include "World.h"

UGridComponent::UGridComponent(const FString& InString) : UPrimitiveComponent(InString)
{
    // PrimitiveType = EPrimitiveType::Grid;
    Topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
}

UGridComponent::~UGridComponent()
{
#ifdef _DEBUG
    if (DynamicVertexBuffer)
    {
        DynamicVertexBuffer->Release();
        DynamicVertexBuffer = nullptr;
    }
#endif
}

void UGridComponent::SetGridStep(float InGridStep)
{
    if (GridStep != InGridStep)
    {
        GridStep = InGridStep;
        bNeedRebuild = true;
    }
}

void UGridComponent::Render(URenderer& renderer)
{
    if (bNeedRebuild)
    {
        RebuildGridLines();

#ifdef _DEBUG
        if (!GridLines.empty())
        {
            uint32 RequiredVertexBufferSize = static_cast<uint32>(GridLines.size() * 2 * sizeof(FVertex));
            if (RequiredVertexBufferSize > VertexBufferSize)
            {
                if (DynamicVertexBuffer)
                {
                    DynamicVertexBuffer->Release();
                }
                VertexBufferSize = (std::max)(RequiredVertexBufferSize, VertexBufferSize * 2);
                DynamicVertexBuffer = renderer.CreateDynamicVertexBuffer(VertexBufferSize);
            }
            std::vector<FVertex> Vertices;
            Vertices.reserve(GridLines.size() * 2);
            for (const FBatchedLine& Line : GridLines)
            {
                Vertices.emplace_back(Line.Start, Line.Color);
                Vertices.emplace_back(Line.End, Line.Color);
            }
            renderer.UpdateDynamicBuffer(DynamicVertexBuffer, Vertices.data(), RequiredVertexBufferSize);
        }
#endif

        bNeedRebuild = false;
    }

#ifndef _DEBUG
    if (!GridLines.empty() && GWorld != nullptr && GWorld->GetLineBatcherComponent() != nullptr)
    {
        GWorld->GetLineBatcherComponent()->DrawLines(GridLines);
    }
#else
    if (!GridLines.empty() && DynamicVertexBuffer != nullptr)
    {
        renderer.SetTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        FConstants constants = {};
        constants.MVPMatrix = FMatrix<float>::Identity();
        renderer.UpdateConstant(constants);
        renderer.DeviceContext->IASetInputLayout(renderer.LineInputLayout);
        renderer.DeviceContext->VSSetShader(renderer.LineVertexShader, nullptr, 0);
        renderer.DeviceContext->PSSetShader(renderer.LinePixelShader, nullptr, 0);
        uint32 offset = 0;
        renderer.DeviceContext->IASetVertexBuffers(0, 1, &DynamicVertexBuffer, &renderer.Stride, &offset);
        renderer.DeviceContext->Draw(static_cast<UINT>(GridLines.size() * 2), 0);
    }
#endif
}

void UGridComponent::RebuildGridLines()
{
    FVector4<float> Color = {0.3f, 0.3f, 0.3f, 0.2f};

    GridLines.clear();
    GridLines.reserve((GridSize * 2 + 1) * 2);

    float GridLength = GridSize * GridStep;

    for (int i = -GridSize; i < 0; ++i)
    {
        GridLines.emplace_back(FVector<float>(-GridLength, i * GridStep, 0.0f),
                               FVector<float>(GridLength, i * GridStep, 0.0f), Color);

        GridLines.emplace_back(FVector<float>(i * GridStep, -GridLength, 0.0f),
                               FVector<float>(i * GridStep, GridLength, 0.0f), Color);
    }

    for (int i = 1; i <= GridSize; ++i)
    {
        GridLines.emplace_back(FVector<float>(-GridLength, i * GridStep, 0.0f),
                               FVector<float>(GridLength, i * GridStep, 0.0f), Color);

        GridLines.emplace_back(FVector<float>(i * GridStep, -GridLength, 0.0f),
                               FVector<float>(i * GridStep, GridLength, 0.0f), Color);
    }

    GridLines.emplace_back(FVector<float>(-GridSize * GridStep, 0.0f, 0.0f), FVector<float>(0.0f, 0.0f, 0.0f), Color);

    GridLines.emplace_back(FVector<float>(0.0f, -GridSize * GridStep, 0.0f), FVector<float>(0.0f, 0.0f, 0.0f), Color);
}