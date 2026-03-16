#include "Engine/Source/Runtime/Engine/Public/Classes/Components/ArrowComponent.h"

UArrowComponent::UArrowComponent() : UPrimitiveComponent()
{
    PrimitiveType = EPrimitiveType::Arrow;
}

UArrowComponent::~UArrowComponent() {}