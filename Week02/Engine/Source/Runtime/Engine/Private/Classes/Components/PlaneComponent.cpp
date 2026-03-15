#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PlaneComponent.h"

UPlaneComponent::UPlaneComponent() 
{
    PrimitiveType = EPrimitiveType::Plane;
    Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

UPlaneComponent::~UPlaneComponent() {}