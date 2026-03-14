#include "Engine/Source/Runtime/Engine/Public/Classes/Components/TriangleComponent.h"
#include "Engine/Source/Runtime/Core/Public/Triangle.h"

// 개별 Component에서 Topology, NumVertices를 결정해야 한다. 
UTriangleComponent::UTriangleComponent() 
{
    Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	SphereRadius = 1.0f;

	Vertices = triangle_vertices;
	NumVertices = sizeof(triangle_vertices) / sizeof(FVertex);
}

UTriangleComponent::~UTriangleComponent() {}