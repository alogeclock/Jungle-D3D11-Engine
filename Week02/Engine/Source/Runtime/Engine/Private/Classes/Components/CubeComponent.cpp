#include "Engine/Source/Runtime/Engine/Public/Classes/Components/CubeComponent.h"

UCubeComponent::UCubeComponent() : UPrimitiveComponent()
{
    PrimitiveType = EPrimitiveType::Cube;
    
}

UCubeComponent::~UCubeComponent() {}