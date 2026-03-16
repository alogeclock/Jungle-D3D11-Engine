#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/TriangleComponent.h"

// 개별 Component에서 Topology, NumVertices를 결정해야 한다. 
UTriangleComponent::UTriangleComponent() : UPrimitiveComponent()
{
    PrimitiveType = EPrimitiveType::Triangle;
}

UTriangleComponent::~UTriangleComponent() {}