#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/AxisComponent.h"

UAxisComponent::UAxisComponent() : UPrimitiveComponent()
{
    PrimitiveType = EPrimitiveType::Axis;
}

UAxisComponent::~UAxisComponent() {}