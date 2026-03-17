#include "Engine/Source/Runtime/Engine/Public/Classes/Components/GridComponent.h"

UGridComponent::UGridComponent() : UPrimitiveComponent()
{
    PrimitiveType = EPrimitiveType::Grid;
    Topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
}

UGridComponent::~UGridComponent() {}