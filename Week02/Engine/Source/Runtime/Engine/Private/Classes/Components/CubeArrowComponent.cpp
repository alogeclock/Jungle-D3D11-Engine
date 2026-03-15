#include "Engine/Source/Runtime/Engine/Public/Classes/Components/CubeArrowComponent.h"

UCubeArrowComponent::UCubeArrowComponent() 
{
    PrimitiveType = EPrimitiveType::CubeArrow;
    Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

UCubeArrowComponent::~UCubeArrowComponent() {}