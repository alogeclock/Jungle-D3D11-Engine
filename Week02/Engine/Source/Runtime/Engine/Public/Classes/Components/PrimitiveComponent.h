#pragma once

#include "Engine\Source\Runtime\Core\Public\Object.h"
#include "Engine\Source\Runtime\Engine\Public\Classes\Components\SceneComponent.h"

class UPrimitiveComponent : public USceneComponent
{
public:
	UPrimitiveComponent() : VertexBuffer(nullptr), NumVertices(0) {}
	virtual ~UPrimitiveComponent()
	{
		if (VertexBuffer)
		{
			VertexBuffer->Release();
			VertexBuffer = nullptr;
		}
	}

	// 렌더러를 통해 GPU 버퍼 생성
	virtual void Init(URenderer& renderer)
	{
		if (Vertices.size() > 0)
		{
			NumVertices = static_cast<uint32>(Vertices.size());
			VertexBuffer = renderer.CreateVertexBuffer(Vertices.data(), sizeof(FVertex) * NumVertices);
		}
	}

	// 그리기
	virtual void RenderPrimitive(URenderer& renderer)
	{
		// 위치, 크기 등 트랜스폼 적용 로직 (renderer.UpdateConstant 등)
		// renderer.UpdateConstant(Location, Scale);

		if (VertexBuffer != nullptr)
		{
			renderer.RenderPrimitiveTopology(VertexBuffer, NumVertices, Topology);
		}
	}

protected:
	ID3D11Buffer* VertexBuffer;
	D3D11_PRIMITIVE_TOPOLOGY Topology;
	uint32 NumVertices;

	// 자식 클래스에서 채워 넣을 정점 배열
	std::vector<FVertex> Vertices; // TArray를 구현하셨다면 TArray<FVertex> 사용
	
	// 트랜스폼 변수 (위치, 회전, 스케일 등)
	// FVector Location;
};

