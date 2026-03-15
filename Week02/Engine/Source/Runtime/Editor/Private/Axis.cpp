#include "Engine/Source/Runtime/Editor/Public/Axis.h"

UAxis::UAxis()
{
	AxisComponent = new UAxisComponent();
}

// [주의] MeshManager에서 버퍼를 관리하므로, 직접 Release할 필요가 없다. 
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