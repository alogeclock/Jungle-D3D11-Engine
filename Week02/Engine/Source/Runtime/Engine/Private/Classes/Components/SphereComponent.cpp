#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/SphereComponent.h"
#include "Engine/Source/Runtime/Core/Public/Sphere.h"

// 개별 USphereComponent에서 Topology, NumVertices를 결정해야 한다. 
USphereComponent::USphereComponent(float InSphereRadius)
{
    Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	SetScale({InSphereRadius, InSphereRadius, InSphereRadius});

	Vertices = sphere_vertices;
	NumVertices = sizeof(sphere_vertices) / sizeof(FVertex);
}

USphereComponent::~USphereComponent() {}