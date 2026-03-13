#pragma once

#include "Engine\Source\Runtime\Editor\Public\Axis.h"

UAxis::UAxis()
{
	//+X축 (빨간색)
	FVertex x1, x2;
	x1.Position = { 0.0f, 0.0f, 0.0f };
	x1.Color = { 1.0f, 0.0f, 0.0f, 1.0f };
	x2.Position = { 1.0f, 0.0f, 0.0f };
	x2.Color = { 1.0f, 0.0f, 0.0f, 1.0f };

	// +y축 (초록색)
	FVertex y1, y2;
	y1.Position = { 0.0f, 0.0f, 0.0f };
	y1.Color = { 0.0f, 1.0f, 0.0f, 1.0f };
	y2.Position = { 0.0f, 1.0f, 0.0f };
	y2.Color = { 0.0f, 1.0f, 0.0f, 1.0f };

	// +z축 (파란색)
	FVertex z1, z2;
	z1.Position = { 0.0f, 0.0f, 0.0f };
	z1.Color = { 0.0f, 0.0f, 1.0f, 1.0f };
	z2.Position = { 0.0f, 0.0f, 1.0f };
	z2.Color = { 0.0f, 0.0f, 1.0f, 1.0f };

	AxisVertices.push_back(x1), AxisVertices.push_back(x2);
	AxisVertices.push_back(y1), AxisVertices.push_back(y2);
	AxisVertices.push_back(z1), AxisVertices.push_back(z2);

	NumVertices = static_cast<int>(AxisVertices.size());
	Topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	
	Color = FVector4(1.0f, 1.0f, 1.0f, 0.0f);
	Location = FVector(0.0f, 0.0f, 0.0f);
	Rotation = FVector(0.0f, 0.0f, 0.0f);
	Scale = 1.0f;
}

UAxis::~UAxis()
{
}

void UAxis::RenderPrimitive(URenderer& renderer)
{
	renderer.UpdateConstant(Location, Scale);
	if (AxisVertexBuffer != nullptr)
	{
		renderer.RenderPrimitiveTopology(AxisVertexBuffer, NumVertices, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	}
}