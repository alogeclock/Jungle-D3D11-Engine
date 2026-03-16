#include "Engine/Source/Runtime/Engine/Public/Classes/Components/RingComponent.h"

URingComponent::URingComponent() : UPrimitiveComponent()
{
    PrimitiveType = EPrimitiveType::Ring;
}

URingComponent::~URingComponent() {}