#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/AxisComponent.h"

UAxisComponent::UAxisComponent(const FString &InString) : UPrimitiveComponent(InString)
{
    PrimitiveType = EPrimitiveType::Axis;
    Topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
}

UAxisComponent::~UAxisComponent() {}