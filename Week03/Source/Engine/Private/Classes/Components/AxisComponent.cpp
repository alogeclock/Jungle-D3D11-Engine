#include "Source/Engine/Public/Classes/Components/AxisComponent.h"
#include "Source/Engine/Public/Classes/Components/LineBatcherComponent.h"
#include "World.h"

UAxisComponent::UAxisComponent(const FString& InString) : UPrimitiveComponent(InString)
{
    PrimitiveType = EPrimitiveType::Axis;
    Topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
}

UAxisComponent::~UAxisComponent()
{
}

void UAxisComponent::SetGridStep(float InGridStep)
{
    if (GridStep != InGridStep)
    {
        GridStep = InGridStep;
        bNeedRebuild = true;
    }
}

void UAxisComponent::Render(URenderer& renderer)
{
    if (bNeedRebuild)
    {
        RebuildAxisLines();
        bNeedRebuild = false;
    }

    if (!AxisLines.empty() && GWorld != nullptr && GWorld->GetLineBatcherComponent() != nullptr)
    {
        GWorld->GetLineBatcherComponent()->DrawLines(AxisLines);
    }
}

void UAxisComponent::RebuildAxisLines()
{
    AxisLines.clear();

    float GridSize = 1000.0f;
    float GridLength = GridSize * GridStep;

    AxisLines.emplace_back(FVector<float>(0.0f, 0.0f, 0.0f), FVector<float>(GridLength, 0.0f, 0.0f), FVector4<float>(1.0f, 0.0f, 0.0f, 1.0f));

    AxisLines.emplace_back(FVector<float>(0.0f, 0.0f, 0.0f), FVector<float>(0.0f, GridLength, 0.0f), FVector4<float>(0.0f, 1.0f, 0.0f, 1.0f));

    AxisLines.emplace_back(FVector<float>(0.0f, 0.0f, 0.0f), FVector<float>(0.0f, 0.0f, GridLength), FVector4<float>(0.0f, 0.0f, 1.0f, 1.0f));
}