#include "Engine/Source/Runtime/Engine/Public/Classes/Components/GridComponent.h"

UGridComponent::UGridComponent(const FString &InString) : UPrimitiveComponent(InString)
{
    PrimitiveType = EPrimitiveType::Grid;
    Topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
}

UGridComponent::~UGridComponent() {}