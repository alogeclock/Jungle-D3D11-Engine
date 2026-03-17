#include "Engine/Source/Runtime/Engine/Public/Classes/Components/CubeComponent.h"

UCubeComponent::UCubeComponent(const FString &InString) : UPrimitiveComponent(InString) {
    PrimitiveType = EPrimitiveType::Cube;
}

UCubeComponent::~UCubeComponent() {}