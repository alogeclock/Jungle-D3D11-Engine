#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PrimitiveComponent.h"

UPrimitiveComponent::UPrimitiveComponent() 
	: VertexBuffer(nullptr), NumVertices(0), Topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
{
}

UPrimitiveComponent::~UPrimitiveComponent()
{
}

void UPrimitiveComponent::Render(URenderer& renderer)
{
	FConstants constants;
	constants.worldMatrix = Transform.ToMatrix();

	renderer.UpdateConstant(constants);

	if (VertexBuffer != nullptr)
	{
		renderer.RenderPrimitiveTopology(VertexBuffer, NumVertices, Topology);
	}
}