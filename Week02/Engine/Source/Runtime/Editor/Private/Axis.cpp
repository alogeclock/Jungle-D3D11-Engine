#include "Engine/Source/Runtime/Editor/Public/Axis.h"

AAxis::AAxis(const FString &InString) : AActor(InString)
{
    USceneComponent *Root = new USceneComponent("AxisSceneComponent");
    SetRootComponent(Root);
    Root->RegisterComponent();

	AxisComponent = new UAxisComponent("AxisPrimitiveComponent");
    AxisComponent->SetOuter(Root);
    AxisComponent->RegisterComponent();
}

AAxis::~AAxis() 
{   
    delete AxisComponent;
    delete this->GetRootComponent();
}

void AAxis::Render(URenderer &renderer)
{
    if (AxisComponent != nullptr)
    {
        AxisComponent->Render(renderer);
    }
}