#include "Source/Engine/Public/Classes/Components/GridComponent.h"
#include "Source/Engine/Public/Classes/Components/LineBatcherComponent.h"
#include "World.h"

UGridComponent::UGridComponent(const FString& InString) : UPrimitiveComponent(InString)
{
    // PrimitiveType = EPrimitiveType::Grid;
    Topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;

    // EditorViewportClient 생성자에서 AGrid가 생성되기 전에 LoadConfig가 호출되므로 여기서 GridStep을 로드함
    char buffer[256];
    LPCSTR iniPath = ".\\editor.ini";
    GetPrivateProfileStringA("GridSettings", "GridStep", "1.0", buffer, sizeof(buffer), iniPath);
    try
    {
        float ParsedGridStep = std::stof(buffer);
        if (ParsedGridStep >= 0.1f && ParsedGridStep <= 10.0f)
        {
            SetGridStep(ParsedGridStep);
        }
        else
        {
            SetGridStep(1.0f);
        }
    }
    catch (const std::exception&)
    {
        SetGridStep(1.0f);
    }
}

UGridComponent::~UGridComponent()
{
    LPCSTR iniPath = ".\\editor.ini";
    std::string gridStepStr = std::to_string(GridStep);
    WritePrivateProfileStringA("GridSettings", "GridStep", gridStepStr.c_str(), iniPath);
}

float UGridComponent::GetGridStep() const
{
    return GridStep;
}

void UGridComponent::SetGridStep(float InGridStep)
{
    if (GridStep == InGridStep)
    {
        return;
    }

    GridStep = InGridStep;
    RebuildGridLines();
}

void UGridComponent::Render(URenderer& renderer)
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

    for (int i = -GridSize; i < 0; ++i)
    {
        GridLines.emplace_back(FVector<float>(-GridSize * GridStep, i * GridStep, 0.0f),
                               FVector<float>(GridSize * GridStep, i * GridStep, 0.0f), Color);

        GridLines.emplace_back(FVector<float>(i * GridStep, -GridSize * GridStep, 0.0f),
                               FVector<float>(i * GridStep, GridSize * GridStep, 0.0f), Color);
    }

    for (int i = 1; i <= GridSize; ++i)
    {
        GridLines.emplace_back(FVector<float>(-GridSize * GridStep, i * GridStep, 0.0f),
                               FVector<float>(GridSize * GridStep, i * GridStep, 0.0f), Color);

        GridLines.emplace_back(FVector<float>(i * GridStep, -GridSize * GridStep, 0.0f),
                               FVector<float>(i * GridStep, GridSize * GridStep, 0.0f), Color);
    }

    GridLines.emplace_back(FVector<float>(-GridSize * GridStep, 0.0f, 0.0f), FVector<float>(0.0f, 0.0f, 0.0f), Color);

    GridLines.emplace_back(FVector<float>(0.0f, -GridSize * GridStep, 0.0f), FVector<float>(0.0f, 0.0f, 0.0f), Color);
}