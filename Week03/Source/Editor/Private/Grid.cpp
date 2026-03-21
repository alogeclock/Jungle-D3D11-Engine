#include "Source/Editor/Public/Grid.h"

AGrid::AGrid(const FString &InString) : AActor(InString)
{
    USceneComponent *Root = new USceneComponent("GridSceneComponent");
    SetRootComponent(Root);
    Root->RegisterComponent();

    GridComponent = new UGridComponent("GridPrimitiveComponent");
    GridComponent->SetIsInEditor(true);
    GridComponent->SetOuter(this);
    GridComponent->RegisterComponent();
}

AGrid::~AGrid()
{
}

void AGrid::Render(URenderer &renderer)
{
    GridComponent->Render(renderer);
}