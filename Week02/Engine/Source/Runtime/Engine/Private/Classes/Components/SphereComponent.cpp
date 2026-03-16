#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/SphereComponent.h"

// 개별 USphereComponent에서 Topology, NumVertices를 결정해야 한다. 
USphereComponent::USphereComponent(float inSphereRadius) : UPrimitiveComponent()
{
    PrimitiveType = EPrimitiveType::Sphere;
	SetScale({inSphereRadius, inSphereRadius, inSphereRadius});
}

USphereComponent::~USphereComponent() {}