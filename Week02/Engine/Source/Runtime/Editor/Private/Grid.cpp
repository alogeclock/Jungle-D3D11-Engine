#include "Engine/Source/Runtime/Editor/Public/Grid.h"

AGrid::AGrid(const FString &InString) : AActor(InString)
{
    USceneComponent *Root = new USceneComponent("GridSceneComponent");
    this->SetRootComponent(Root);
    Root->RegisterComponent();

    GridComponent = new UGridComponent("GridPrimitiveComponent");
    GridComponent->SetOuter(this);
    GridComponent->RegisterComponent();
}

AGrid::~AGrid()
{
}

void AGrid::Render(URenderer &renderer)
{
    FMatrix<float> TargetMatrix = GetTransform().ToMatrix();
    GridComponent->SetParentMatrix(TargetMatrix);
    GridComponent->Render(renderer);
}