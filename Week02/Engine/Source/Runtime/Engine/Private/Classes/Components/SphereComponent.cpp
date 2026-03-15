#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/SphereComponent.h"

// 개별 USphereComponent에서 Topology, NumVertices를 결정해야 한다. 
USphereComponent::USphereComponent(float inSphereRadius)
{
    PrimitiveType = EPrimitiveType::Sphere;
    Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	SetScale({inSphereRadius, inSphereRadius, inSphereRadius});
}

USphereComponent::~USphereComponent() {}