#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/AxisComponent.h"

UAxisComponent::UAxisComponent()
{
	// +x축 (빨간색)
	AxisVertices.push_back({{ 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }}); 
	AxisVertices.push_back({{ 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }});
	
	// +y축 (초록색)
	AxisVertices.push_back({{ 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }});
	AxisVertices.push_back({{ 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f }});
	
	// +z축 (파란색)
	AxisVertices.push_back({{ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }}); 
	AxisVertices.push_back({{ 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f }});

	NumVertices = static_cast<int>(AxisVertices.size());
	Topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	
	Color = FVector4(1.0f, 1.0f, 1.0f, 0.0f);
	Location = FVector(0.0f, 0.0f, 0.0f);
	Rotation = FVector(0.0f, 0.0f, 0.0f);
	Scale = 1.0f;
}

UAxisComponent::~UAxisComponent()
{
}

void UAxisComponent::RenderPrimitive(URenderer& renderer)
{
	//renderer.UpdateConstant(Location, Scale);
	if (AxisVertexBuffer != nullptr)
	{
		renderer.RenderPrimitiveTopology(AxisVertexBuffer, NumVertices, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	}
}