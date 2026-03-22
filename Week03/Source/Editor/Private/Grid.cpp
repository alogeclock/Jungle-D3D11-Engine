#include "Source/Editor/Public/Grid.h"

AGrid::AGrid(const FString &InString) : AActor(InString)
{
    USceneComponent *Root = new USceneComponent("GridSceneComponent");
    SetRootComponent(Root);
    AddOwnedComponent(Root);
    Root->RegisterComponent();

    GridComponent = new UGridComponent("GridPrimitiveComponent");
    GridComponent->SetIsInEditor(true);
    GridComponent->SetOuter(this);
    AddOwnedComponent(GridComponent);
    GridComponent->RegisterComponent();

    char   buffer[256];
    LPCSTR iniPath = ".\\editor.ini";

    // TODO: 여기에 있어도 될 로직은 아닌 듯 함
    // EditorViewportClient 생성자에서 AGrid가 생성되기 전에 LoadConfig가 호출되므로 일단 여기서 GridStep을 로드하도록 함
    GetPrivateProfileStringA("GridSettings", "GridStep", "1.0", buffer, sizeof(buffer), iniPath);
    try
    {
        float ParsedGridStep = std::stof(buffer);
        if (ParsedGridStep >= 0.1f && ParsedGridStep <= 10.0f)
            SetGridStep(ParsedGridStep);
        else
            SetGridStep(1.0f);
    }
    catch (const std::exception &)
    {
        SetGridStep(1.0f);
    }
}

AGrid::~AGrid()
{
}

void AGrid::Render(URenderer &renderer)
{
}