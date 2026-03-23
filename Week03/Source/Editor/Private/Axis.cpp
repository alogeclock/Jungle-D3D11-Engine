#include "Source/Editor/Public/Axis.h"

AAxis::AAxis(const FString &InString) : AActor(InString)
{
    USceneComponent *Root = new USceneComponent("AxisSceneComponent");
    SetRootComponent(Root);
    AddOwnedComponent(Root);
    Root->RegisterComponent();

	AxisComponent = new UAxisComponent("AxisPrimitiveComponent");
    AxisComponent->SetOuter(Root);
    AxisComponent->SetIsInEditor(true);
    AddOwnedComponent(AxisComponent);
    AxisComponent->RegisterComponent();
}

AAxis::~AAxis()
{
}