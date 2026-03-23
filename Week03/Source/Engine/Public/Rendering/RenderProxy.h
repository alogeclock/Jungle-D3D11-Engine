#pragma once
#include "Source/Engine/Public/Rendering/Renderer.h"

enum class ERenderCommandType
{
    StaticPrimitive, // 미리 로드된 Vertex, Index Buffer를 쓰는 타입 (Cube, Sphere)
    DynamicPrimitive // 런타임에 데이터가 바뀌는 Buffer를 쓰는 타입 (LineBatcher 등)
};

struct FRenderCommand
{
    ERenderCommandType CommandType = ERenderCommandType::StaticPrimitive;

    // 공통 렌더 스테이트
    FConstants Constants;
    FConstantsColor ConstantsColor;
    D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    ECullMode CullMode = ECullMode::Back;
    bool bEnableDepthTest = true;

    // StaticPrimitive 용
    EPrimitiveType PrimitiveType = EPrimitiveType::None;

    // DynamicIndexed 용
    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer = nullptr;
    uint32 NumIndices = 0;
    uint32 Stride = 0;
};

class FRenderProxy
{
public:
    virtual ~FRenderProxy() = default;
    
    // 렌더러의 커맨드 큐에 자신을 그리는 커맨드를 조립하여 삽입
    virtual void GenerateRenderCommand(class URenderer& Renderer) = 0;
};

class FStaticMeshRenderProxy : public FRenderProxy
{
public:
    FConstants Constants;
    FConstantsColor ConstantsColor;
    EPrimitiveType PrimitiveType;
    D3D11_PRIMITIVE_TOPOLOGY Topology;
    ECullMode CullMode;
    bool bEnableDepthTest;

    virtual void GenerateRenderCommand(URenderer& Renderer) override;
};

class FDynamicMeshRenderProxy : public FRenderProxy
{
public:
    FConstants Constants;
    FConstantsColor ConstantsColor;
    D3D11_PRIMITIVE_TOPOLOGY Topology;
    ECullMode CullMode;
    bool bEnableDepthTest;

    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer = nullptr;
    uint32 NumIndices = 0;
    uint32 Stride = 0;

    virtual void GenerateRenderCommand(URenderer& Renderer) override;
};