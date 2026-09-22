#include "Engine/Source/Runtime/Engine/Public/Classes/Components/RingComponent.h"

URingComponent::URingComponent(const FString &InString) : UPrimitiveComponent(InString) {
    PrimitiveType = EPrimitiveType::Ring;
}

URingComponent::~URingComponent() {}