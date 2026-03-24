#pragma once

#include <d3d11.h>
#include "Source/Core/Public/Math/Matrix.h"
#include "CoreTypes.h"

struct FConstants
{
    FMatrix<float> MVPMatrix;
};

struct FConstantsColor
{
    float r, g, b, a;
};

class URenderer;

enum class ERenderCommandType
{
    StaticPrimitive, // 미리 로드된 Vertex, Index Buffer를 쓰는 타입 (Cube, Sphere)
    DynamicPrimitive // 런타임에 데이터가 바뀌는 Buffer를 쓰는 타입 (LineBatcher 등)
};

struct FRenderCommand
{
    FConstants Constants;
    FConstantsColor ConstantsColor;
    D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    ECullMode CullMode = ECullMode::Back;
    bool bEnableDepthTest = true;
    bool bIsVisible = true;
    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer = nullptr;
    uint32 NumVertices = 0;
    uint32 NumIndices = 0;
    uint32 Stride = 0;
};

class FRenderProxy
{
public:
    FRenderCommand RenderCommand;
    virtual ~FRenderProxy() = default;
};