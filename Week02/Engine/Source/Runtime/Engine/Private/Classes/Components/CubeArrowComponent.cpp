#include "Engine/Source/Runtime/Engine/Public/Classes/Components/CubeArrowComponent.h"

UCubeArrowComponent::UCubeArrowComponent(const FString &InString) : UPrimitiveComponent(InString) {
    PrimitiveType = EPrimitiveType::CubeArrow;
}

UCubeArrowComponent::~UCubeArrowComponent() {}