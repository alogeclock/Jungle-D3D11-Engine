#include "Engine/Source/Runtime/Engine/Public/Classes/Components/CubeComponent.h"

UCubeComponent::UCubeComponent() 
{
    PrimitiveType = EPrimitiveType::Cube;
    Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

UCubeComponent::~UCubeComponent() {}