
#pragma once

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/SceneComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Rendering/Renderer.h"

class UPrimitiveComponent : public USceneComponent
{
  public:
    UPrimitiveComponent();
    virtual ~UPrimitiveComponent();

    virtual void Render(URenderer &renderer);
    
	void SetVertexBuffer(ID3D11Buffer* InBuffer) { VertexBuffer = InBuffer; }
	ID3D11Buffer* GetVertexBuffer() const { return VertexBuffer; }

    const FVertex* GetVertices() { return Vertices; }
    uint32 GetVertexByteWidth() const { return NumVertices * sizeof(FVertex); }
    uint32 GetNumVertices() const { return NumVertices; }

  protected:
    ID3D11Buffer            *VertexBuffer;
    D3D11_PRIMITIVE_TOPOLOGY Topology;
    uint32                   NumVertices;

    const FVertex* Vertices; // CPU에서 관리하는 정점 데이터 배열, Init()에서 GPU 버퍼로 업로드
};