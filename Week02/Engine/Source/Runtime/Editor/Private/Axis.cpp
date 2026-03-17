#include "Engine/Source/Runtime/Editor/Public/Axis.h"

AAxis::AAxis()
{
    USceneComponent *Root = new USceneComponent();
    SetRootComponent(Root);
    Root->RegisterComponent();

	AxisComponent = new UAxisComponent();
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