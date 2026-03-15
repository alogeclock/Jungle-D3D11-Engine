#include "Engine/Source/Runtime/Engine/Public/Classes/Components/RingComponent.h"

URingComponent::URingComponent() 
{
    PrimitiveType = EPrimitiveType::Ring;
    Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

URingComponent::~URingComponent() {}