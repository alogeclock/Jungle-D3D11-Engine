#include "Engine/Source/Runtime/Editor/Public/Axis.h"

UAxis::UAxis()
{
	AxisComponent = new UAxisComponent();
}

UAxis::~UAxis() 
{
}

void UAxis::Render(URenderer &renderer)
{
    if (AxisComponent != nullptr)
    {
        AxisComponent->Render(renderer);
    }
}