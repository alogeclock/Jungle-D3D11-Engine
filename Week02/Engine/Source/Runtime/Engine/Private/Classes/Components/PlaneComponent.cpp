#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PlaneComponent.h"

UPlaneComponent::UPlaneComponent(const FString &InString) : UPrimitiveComponent(InString) {
    PrimitiveType = EPrimitiveType::Plane;
}

UPlaneComponent::~UPlaneComponent() {}