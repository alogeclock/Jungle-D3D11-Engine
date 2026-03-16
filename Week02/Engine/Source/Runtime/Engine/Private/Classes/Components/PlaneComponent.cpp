#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PlaneComponent.h"

UPlaneComponent::UPlaneComponent() : UPrimitiveComponent()
{
    PrimitiveType = EPrimitiveType::Plane;
}

UPlaneComponent::~UPlaneComponent() {}