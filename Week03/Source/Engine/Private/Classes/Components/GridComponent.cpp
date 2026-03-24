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
        bNeedRebuild = false;
    }

    if (!GridLines.empty() && GWorld != nullptr && GWorld->GetLineBatcherComponent() != nullptr)
    {
        GWorld->GetLineBatcherComponent()->DrawLines(GridLines);
    }
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