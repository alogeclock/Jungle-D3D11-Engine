#pragma once

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#include <d3d11.h>
#include <d3dcompiler.h>

#include "Source/Core/Public/Math/Vector.h"
#include "Source/Core/Public/Math/Vector4.h"
#include "Source/Core/Public/Math/Matrix.h"
#include "Source/Core/Public/Math/Box.h"
#include "Source/Core/Public/Math/TranslationMatrix.h"
#include "Source/Core/Public/Math/PerspectiveMatrix.h"
#include "Source/Core/Public/Math/OrthographicMatrix.h"

#include "CoreTypes.h"

#include "Source/Engine/Public/Rendering/RenderProxy.h"
#include <wrl.h>

using namespace Microsoft::WRL;

class FViewport;
class UMeshManager;
class UPrimitiveComponent;

struct FTextGpuBuffer
{
    ComPtr<ID3D11Buffer> VertexBuffer;
    ComPtr<ID3D11Buffer> IndexBuffer;
    uint32 VertexBufferSize = 0;
    uint32 IndexBufferSize = 0;
};

class URenderer
{
  public:
    URenderer();
    ~URenderer();

  public:
    ID3D11Device        *Device = nullptr;
    ID3D11DeviceContext *DeviceContext = nullptr;
    IDXGISwapChain      *SwapChain = nullptr;

    ID3D11Texture2D        *FrameBuffer = nullptr;
    ID3D11Texture2D        *DepthStencilBuffer = nullptr;
    ID3D11DepthStencilView *DepthStencilView = nullptr;
    ID3D11RenderTargetView *FrameBufferRTV = nullptr;

    ID3D11Buffer *ConstantBuffer = nullptr;
    ID3D11Buffer *ConstantBufferColor = nullptr;

    // 깊이 스텐실 상태 객체
    ID3D11DepthStencilState *DepthStateDefault = nullptr; // 일반적인 3D 렌더링용 (Depth 켬)
    ID3D11DepthStencilState *DepthStateIgnore = nullptr;  // 기즈모, UI용 (Depth 끔)

    ID3D11RasterizerState *RasterizerStateCullBack = nullptr;
    ID3D11RasterizerState *RasterizerStateCullFront = nullptr;
    ID3D11RasterizerState *RasterizerStateCullNone = nullptr;
    ID3D11RasterizerState *RasterizerStateWireframe = nullptr; // 임시

    ID3D11BlendState *BlendState = nullptr;
    FLOAT             BlendFactor[4] = {0.f, 0.f, 0.f, 0.f};
    FLOAT             ClearColor[4] = {0.025f, 0.025f, 0.025f, 1.0f}; // 화면을 초기화하는 색

    D3D11_VIEWPORT           ViewportInfo;
    D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    ID3D11VertexShader *SimpleVertexShader;
    ID3D11VertexShader *TextVertexShader;
    ID3D11VertexShader *LineVertexShader;
    ID3D11PixelShader  *SimplePixelShader;
    ID3D11PixelShader  *TextPixelShader;
    ID3D11PixelShader  *LinePixelShader;
    ID3D11InputLayout  *SimpleInputLayout;
    ID3D11InputLayout  *TextInputLayout;
    ID3D11InputLayout  *LineInputLayout;

    ID3D11SamplerState *LinearSamplerState;

    unsigned int Stride;

  public:
    void Create(HWND hWindow);
    void Release();

    void CreateFrameBuffer();
    void ReleaseFrameBuffer();

    void CreateRasterizerState();
    void ReleaseRasterizerState();

    void CreateDeviceAndSwapChain(HWND hWindow);
    void ReleaseDeviceAndSwapChain();
    void SwapBuffer();

    void CreateShader();
    void CreateTextShader();
    void CreateLineShader();
    void ReleaseShader();

    void CreateDepthStencilBuffer(uint32 width, uint32 height);
    void ReleaseDepthStencilBuffer();

    void CreateDepthStencilState();
    void ReleaseDepthStencilState();
    void SetDepthStencilEnable(bool bEnable);

    void SetCullMode(ECullMode cullMode);
    void SetViewMode(EViewModeIndex Mode);
    void SetShowFlags(EEngineShowFlags ShowFlags);
    void ApplyRasterizerState();

    bool CheckShowFlag(EEngineShowFlags InFlag) const { return (ShowFlags & InFlag) != EEngineShowFlags::None; }

    void SetDrawAABB(bool bEnable) { bDrawAABB = bEnable; }
    bool IsDrawAABB() const { return bDrawAABB; }

    void CreateBlendState();
    void ReleaseBlendState();

    void Prepare(const FSceneViewOptions &ViewOptions);
    void PrepareShader();

    void EnsureTextBuffers(
        FTextGpuBuffer& Buffers,
        uint32 RequiredVBSize,
        uint32 RequiredIBSize);

    void UploadTextBuffers(
        FTextGpuBuffer& Buffers,
        const TArray<FTextVertex>& Vertices,
        const TArray<uint32>& Indices);

    void DrawTextBuffers(
        const FString& FontPath,
        const FConstants& Constants,
        const FTextGpuBuffer& Buffers,
        uint32 IndexCount);

    void RenderTextBatch(
        const FString& FontPath,
        const FConstants& Constants,
        const TArray<FTextVertex>& Vertices,
        const TArray<uint32>& Indices,
        FTextGpuBuffer& Buffers);

    void UndoRenderText();

    void RenderPrimitive(UPrimitiveComponent *primitive, FConstants &constants, FConstantsColor &constantsColor);

    void Draw(ID3D11Buffer *vertexBuffer, uint32 numVertices, uint32 InStride);
    void DrawIndexed(ID3D11Buffer *vertexBuffer, ID3D11Buffer *indexBuffer, uint32 numIndices, uint32 InStride);

    ID3D11Buffer *CreateVertexBuffer(const FVertex *vertices, uint32 byteWidth);
    void          ReleaseVertexBuffer(ID3D11Buffer *vertexBuffer);

    ID3D11Buffer *CreateIndexBuffer(const uint16 *indices, uint32 byteWidth);
    void          ReleaseIndexBuffer(ID3D11Buffer *indexBuffer);

    ID3D11Buffer *CreateDynamicVertexBuffer(uint32 byteWidth);
    void          ReleaseDynamicVertexBuffer(ID3D11Buffer *vertexBuffer);

    ID3D11Buffer *CreateDynamicIndexBuffer(uint32 byteWidth);
    void          ReleaseDynamicIndexBuffer(ID3D11Buffer *indexBuffer);

    void UpdateDynamicBuffer(ID3D11Buffer *Buffer, const void *Data, uint32 byteWidth);

    void CreateConstantBuffer();
    void ReleaseConstantBuffer();

    void UpdateConstant(FConstants data);
    void UpdateConstant(FConstantsColor data);

    // Text billboard용 카메라 축 추출
    bool GetCameraBasis(FVector<float> &OutRight, FVector<float> &OutUp, FVector<float> &OutForward) const;

    void SetViewport(FViewport *viewport) { Viewport = viewport; };
    void OnResize(uint32 NewWidth, uint32 NewHeight);

    void SetTopology(D3D11_PRIMITIVE_TOPOLOGY InTopology);

    // 프록시로부터 렌더 커맨드를 받아 큐에 쌓는 함수
    void SubmitCommand(const FRenderCommand& Command) { RenderQueue.push_back(Command); }
    void Flush() { RenderQueue.clear(); }

  private:
    FViewport       *Viewport = nullptr;
    ECullMode        CurrentCullMode = ECullMode::Back;
    EViewModeIndex   ViewModeIndex = EViewModeIndex::VMI_Lit;
    EEngineShowFlags ShowFlags = EEngineShowFlags::SF_Primitives | EEngineShowFlags::SF_UUID;
    bool             bDrawAABB = false;
    
    TArray<FRenderCommand> RenderQueue;
};