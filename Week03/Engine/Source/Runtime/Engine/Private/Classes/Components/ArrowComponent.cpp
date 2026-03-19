#include "Engine/Source/Runtime/Engine/Public/Classes/Components/ArrowComponent.h"

UArrowComponent::UArrowComponent(const FString &InString) : UPrimitiveComponent(InString) {
    PrimitiveType = EPrimitiveType::Arrow;
}

UArrowComponent::~UArrowComponent() {}