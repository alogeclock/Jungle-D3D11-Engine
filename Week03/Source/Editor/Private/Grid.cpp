#include "Source/Editor/Public/Grid.h"

AGrid::AGrid(const FString& InString) : AActor(InString)
{
    USceneComponent* Root = new USceneComponent("GridSceneComponent");
    SetRootComponent(Root);
    AddOwnedComponent(Root);
    Root->RegisterComponent();

    GridComponent = new UGridComponent("GridPrimitiveComponent");
    GridComponent->SetIsInEditor(true);
    GridComponent->SetOuter(this);
    AddOwnedComponent(GridComponent);
    GridComponent->RegisterComponent();
}

AGrid::~AGrid()
{
}
