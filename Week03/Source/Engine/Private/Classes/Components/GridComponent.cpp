#include "Source/Engine/Public/Classes/Components/GridComponent.h"
#include "Source/Engine/Public/Classes/Components/LineBatcherComponent.h"
#include "World.h"

UGridComponent::UGridComponent(const FString &InString) : UPrimitiveComponent(InString)
{
    PrimitiveType = EPrimitiveType::Grid;
    Topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
}

UGridComponent::~UGridComponent() {}

void UGridComponent::SetGridStep(float InGridStep)
{
    if (GridStep == InGridStep)
        return;

    GridStep = InGridStep;
    RebuildGridLines();
}

void UGridComponent::Render(URenderer &renderer)
{
    if (!bInitialized)
    {
        RebuildGridLines();
        bInitialized = true;
    }

    if (!GridLines.empty() && GWorld != nullptr && GWorld->GetLineBatcherComponent() != nullptr)
    {
        GWorld->GetLineBatcherComponent()->DrawLines(std::span<const FBatchedLine>(GridLines.data(), GridLines.size()));
    }
}

void UGridComponent::RebuildGridLines()
{
    FVector4<float> Color = {0.3f, 0.3f, 0.3f, 0.2f};

    GridLines.clear();
    GridLines.reserve((GridSize * 2 + 1) * 2);

    for (int i = -GridSize; i <= GridSize; ++i)
    {
        // 가로선 (X축과 평행)
        GridLines.emplace_back(
            FVector<float>(-GridSize * GridStep, i * GridStep, -0.005f),
            FVector<float>(GridSize * GridStep, i * GridStep, -0.005f),
            Color
        );

        // 세로선 (Z축과 평행)
        GridLines.emplace_back(
            FVector<float>(i * GridStep, -GridSize * GridStep, -0.005f),
            FVector<float>(i * GridStep, GridSize * GridStep, -0.005f),
            Color
        );
    }
}