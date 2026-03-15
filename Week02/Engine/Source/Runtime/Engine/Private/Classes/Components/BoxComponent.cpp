#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/BoxComponent.h"
#include "Engine/Source/Runtime/Core/Public/Cube.h"

// 개별 Component에서 Topology, NumVertices를 결정해야 한다. 
UBoxComponent::UBoxComponent() 
{
    Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	Vertices = cube_vertices;
	NumVertices = sizeof(cube_vertices) / sizeof(FVertex);
}

UBoxComponent::~UBoxComponent() {}