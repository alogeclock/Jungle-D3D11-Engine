#include "Engine/Source/Runtime/Engine/Public/Classes/Components/ArrowComponent.h"

UArrowComponent::UArrowComponent() 
{
    PrimitiveType = EPrimitiveType::Arrow;
    Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

UArrowComponent::~UArrowComponent() {}