#include "Engine/Source/Runtime/Editor/Public/Axis.h"

AAxis::AAxis()
{
	AxisComponent = new UAxisComponent();
    AxisComponent->SetCullMode(ECullMode::None);
}

AAxis::~AAxis() 
{
    delete AxisComponent;
}

void AAxis::Render(URenderer &renderer)
{
    if (AxisComponent != nullptr)
    {
        AxisComponent->Render(renderer);
    }
}