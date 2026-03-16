#include "Engine/Source/Runtime/Engine/Public/Classes/Components/TorusComponent.h"

// 개별 Component에서 Topology, NumVertices를 결정해야 한다. 
UTorusComponent::UTorusComponent() : UPrimitiveComponent()
{
    PrimitiveType = EPrimitiveType::Torus;
}

UTorusComponent::~UTorusComponent() {}