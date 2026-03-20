#include "Source/Runtime/Core/Public/Memory.h"
#include "Source/Runtime/Engine/Public/Classes/Components/TriangleComponent.h"

// 개별 Component에서 Topology, NumVertices를 결정해야 한다. 
UTriangleComponent::UTriangleComponent(const FString &InString) : UPrimitiveComponent(InString) {
    PrimitiveType = EPrimitiveType::Triangle;
}

UTriangleComponent::~UTriangleComponent() {}