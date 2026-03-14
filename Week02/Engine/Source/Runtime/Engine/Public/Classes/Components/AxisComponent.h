#pragma once

#include "CoreTypes.h"
#include "Object\Object.h"
#include "Engine\Source\Runtime\Engine\Public\Rendering\Renderer.h"

class UAxisComponent : public UObject
{
public:
	UAxisComponent();
	~UAxisComponent() override;

	void RenderPrimitive(URenderer& renderer);
	void SetVertexBuffer(ID3D11Buffer* InBuffer) { AxisVertexBuffer = InBuffer; }
	ID3D11Buffer* GetVertexBuffer() const { return AxisVertexBuffer; }
	
	FVertex* GetVertexData() { return AxisVertices.data(); }
	uint32 GetVertexByteWidth() const { return static_cast<uint32>(AxisVertices.size() * sizeof(FVertex)); }
	uint32 GetNumVertices() const { return NumVertices; }

private:
	FVector4<float> Color;
	FVector<float> Location;
	FVector<float> Rotation;
	float Scale;

	ID3D11Buffer* AxisVertexBuffer = nullptr;
	D3D11_PRIMITIVE_TOPOLOGY Topology;

	uint32 NumVertices;
	TArray<FVertex> AxisVertices;
};