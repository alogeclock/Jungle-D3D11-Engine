#include "Source/Engine/Public/Rendering/Renderer.h"
#include "Source/Core/Public/Memory.h"
#include "Source/Editor/Public/Viewport.h"

URenderer::URenderer()
{
}

URenderer::~URenderer()
{
}

void URenderer::Create(HWND hWindow)
{
    CreateDeviceAndSwapChain(hWindow);
    CreateFrameBuffer();
    CreateRasterizerState();

    CreateConstantBuffer();
    CreateDepthStencilBuffer(ViewportInfo.Width, ViewportInfo.Height);
    CreateDepthStencilState();

    CreateShader();
    CreateTextShader();
    CreateLineShader();
    CreateBlendState();
}

void URenderer::CreateDeviceAndSwapChain(HWND hWindow)
{
    D3D_FEATURE_LEVEL featurelevels[] = {D3D_FEATURE_LEVEL_11_0};

    DXGI_SWAP_CHAIN_DESC swapchaindesc = {};
    swapchaindesc.BufferDesc.Width = 0;
    swapchaindesc.BufferDesc.Height = 0;
    swapchaindesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapchaindesc.SampleDesc.Count = 1;
    swapchaindesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchaindesc.BufferCount = 2;
    swapchaindesc.OutputWindow = hWindow;
    swapchaindesc.Windowed = TRUE;
    swapchaindesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                  D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG, featurelevels,
                                  ARRAYSIZE(featurelevels), D3D11_SDK_VERSION, &swapchaindesc, &SwapChain, &Device,
                                  nullptr, &DeviceContext);

    SwapChain->GetDesc(&swapchaindesc);

    ViewportInfo = {0.0f, 0.0f, (float)swapchaindesc.BufferDesc.Width, (float)swapchaindesc.BufferDesc.Height,
                    0.0f, 1.0f};
}

void URenderer::ReleaseDeviceAndSwapChain()
{
    if (DeviceContext)
    {
        DeviceContext->Flush();
    }

    if (SwapChain)
    {
        SwapChain->Release();
        SwapChain = nullptr;
    }

    if (Device)
    {
        Device->Release();
        Device = nullptr;
    }

    if (DeviceContext)
    {
        DeviceContext->Release();
        DeviceContext = nullptr;
    }
}

void URenderer::CreateFrameBuffer()
{
    SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);

    D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
    framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    Device->CreateRenderTargetView(FrameBuffer, &framebufferRTVdesc, &FrameBufferRTV);
}

void URenderer::ReleaseFrameBuffer()
{
    if (FrameBuffer)
    {
        FrameBuffer->Release();
        FrameBuffer = nullptr;
    }

    if (FrameBufferRTV)
    {
        FrameBufferRTV->Release();
        FrameBufferRTV = nullptr;
    }
}

void URenderer::CreateRasterizerState()
{
    CD3D11_RASTERIZER_DESC rasterizerdesc(D3D11_DEFAULT);
    rasterizerdesc.FillMode = D3D11_FILL_SOLID;

    rasterizerdesc.FrontCounterClockwise = true;

    rasterizerdesc.CullMode = D3D11_CULL_BACK;
    Device->CreateRasterizerState(&rasterizerdesc, &RasterizerStateCullBack);

    rasterizerdesc.CullMode = D3D11_CULL_FRONT;
    Device->CreateRasterizerState(&rasterizerdesc, &RasterizerStateCullFront);

    rasterizerdesc.CullMode = D3D11_CULL_NONE;
    Device->CreateRasterizerState(&rasterizerdesc, &RasterizerStateCullNone);

    rasterizerdesc.FillMode = D3D11_FILL_WIREFRAME;
    rasterizerdesc.CullMode = D3D11_CULL_NONE;
    Device->CreateRasterizerState(&rasterizerdesc, &RasterizerStateWireframe);
}

void URenderer::ReleaseRasterizerState()
{
    if (RasterizerStateCullBack)
    {
        RasterizerStateCullBack->Release();
        RasterizerStateCullBack = nullptr;
    }
    if (RasterizerStateCullFront)
    {
        RasterizerStateCullFront->Release();
        RasterizerStateCullFront = nullptr;
    }
    if (RasterizerStateCullNone)
    {
        RasterizerStateCullNone->Release();
        RasterizerStateCullNone = nullptr;
    }
    if (RasterizerStateWireframe)
    {
        RasterizerStateWireframe->Release();
        RasterizerStateWireframe = nullptr;
    }
}

void URenderer::Release()
{
    ReleaseRasterizerState();

    DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

    ReleaseFrameBuffer();
    ReleaseDeviceAndSwapChain();

    ReleaseConstantBuffer();
    ReleaseDepthStencilBuffer();
    ReleaseDepthStencilState();

    ReleaseShader();

    ReleaseBlendState();
}

void URenderer::SwapBuffer()
{
    SwapChain->Present(0, 0); // 1: Vsync 활성화
}

void URenderer::CreateShader()
{
    ID3DBlob* vertexshaderCSO = nullptr;
    ID3DBlob* pixelshaderCSO = nullptr;

    D3DCompileFromFile(L"Shaders/ShaderW0.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &vertexshaderCSO, nullptr);

    Device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), nullptr,
                               &SimpleVertexShader);

    D3DCompileFromFile(L"Shaders/ShaderW0.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &pixelshaderCSO, nullptr);

    Device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(), pixelshaderCSO->GetBufferSize(), nullptr,
                              &SimplePixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0,    DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {   "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    Device->CreateInputLayout(layout, ARRAYSIZE(layout), vertexshaderCSO->GetBufferPointer(),
                              vertexshaderCSO->GetBufferSize(), &SimpleInputLayout);

    Stride = sizeof(FVertex);

    vertexshaderCSO->Release();
    pixelshaderCSO->Release();
}

void URenderer::CreateTextShader()
{
    ID3DBlob* textVS = nullptr;
    ID3DBlob* textPS = nullptr;

    D3DCompileFromFile(L"Shaders/ShaderFont.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &textVS, nullptr);
    Device->CreateVertexShader(textVS->GetBufferPointer(), textVS->GetBufferSize(), nullptr, &TextVertexShader);

    D3DCompileFromFile(L"Shaders/ShaderFont.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &textPS, nullptr);
    Device->CreatePixelShader(textPS->GetBufferPointer(), textPS->GetBufferSize(), nullptr, &TextPixelShader);

    D3D11_INPUT_ELEMENT_DESC TextLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0,    DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    Device->CreateInputLayout(TextLayout, ARRAYSIZE(TextLayout), textVS->GetBufferPointer(), textVS->GetBufferSize(),
                              &TextInputLayout);

    // 샘플러
    D3D11_SAMPLER_DESC SamplerDesc = {};
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

    Device->CreateSamplerState(&SamplerDesc, &LinearSamplerState);

    textVS->Release();
    textPS->Release();
}

void URenderer::CreateLineShader()
{
    ID3DBlob* lineVS = nullptr;
    ID3DBlob* linePS = nullptr;

    D3DCompileFromFile(L"Shaders/LineShader.hlsl", nullptr, nullptr, "VS", "vs_5_0", 0, 0, &lineVS, nullptr);
    Device->CreateVertexShader(lineVS->GetBufferPointer(), lineVS->GetBufferSize(), nullptr, &LineVertexShader);

    D3DCompileFromFile(L"Shaders/LineShader.hlsl", nullptr, nullptr, "PS", "ps_5_0", 0, 0, &linePS, nullptr);
    Device->CreatePixelShader(linePS->GetBufferPointer(), linePS->GetBufferSize(), nullptr, &LinePixelShader);

    D3D11_INPUT_ELEMENT_DESC LineLayout[] = {
        {"POSITION", 0,    DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {   "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    Device->CreateInputLayout(LineLayout, ARRAYSIZE(LineLayout), lineVS->GetBufferPointer(), lineVS->GetBufferSize(),
                              &LineInputLayout);

    lineVS->Release();
    linePS->Release();
}

void URenderer::ReleaseShader()
{
    if (SimpleInputLayout)
    {
        SimpleInputLayout->Release();
        SimpleInputLayout = nullptr;
    }

    if (SimpleVertexShader)
    {
        SimpleVertexShader->Release();
        SimpleVertexShader = nullptr;
    }

    if (SimplePixelShader)
    {
        SimplePixelShader->Release();
        SimplePixelShader = nullptr;
    }

    if (TextInputLayout)
    {
        TextInputLayout->Release();
        TextInputLayout = nullptr;
    }
    if (TextVertexShader)
    {
        TextVertexShader->Release();
        TextVertexShader = nullptr;
    }
    if (TextPixelShader)
    {
        TextPixelShader->Release();
        TextPixelShader = nullptr;
    }
    if (LineInputLayout)
    {
        LineInputLayout->Release();
        LineInputLayout = nullptr;
    }
    if (LineVertexShader)
    {
        LineVertexShader->Release();
        LineVertexShader = nullptr;
    }
    if (LinePixelShader)
    {
        LinePixelShader->Release();
        LinePixelShader = nullptr;
    }
    if (LinearSamplerState)
    {
        LinearSamplerState->Release();
        LinearSamplerState = nullptr;
    }
}

void URenderer::CreateDepthStencilBuffer(uint32 width, uint32 height)
{
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    Device->CreateTexture2D(&depthDesc, nullptr, &DepthStencilBuffer);

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

    Device->CreateDepthStencilView(DepthStencilBuffer, &dsvDesc, &DepthStencilView);
}

void URenderer::ReleaseDepthStencilBuffer()
{
    if (DepthStencilBuffer) 
    {
        DepthStencilBuffer->Release();
        DepthStencilBuffer = nullptr;
    }
    if (DepthStencilView)
    {
        DepthStencilView->Release();
        DepthStencilView = nullptr;
    }
}

void URenderer::CreateDepthStencilState()
{
    // 1. 기본 상태 (깊이 판정 켜기)
    D3D11_DEPTH_STENCIL_DESC dsDescDefault = {};
    dsDescDefault.DepthEnable = TRUE;
    dsDescDefault.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDescDefault.DepthFunc = D3D11_COMPARISON_LESS; // 앞쪽에 있는 픽셀만 그리기

    Device->CreateDepthStencilState(&dsDescDefault, &DepthStateDefault);

    // 2. 무시 상태 (깊이 판정 끄기)
    D3D11_DEPTH_STENCIL_DESC dsDescIgnore = {};
    dsDescIgnore.DepthEnable = FALSE;                          // 핵심: Depth 테스트를 아예 수행하지 않음
    dsDescIgnore.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // Depth 버퍼에 쓰기도 방지
    dsDescIgnore.DepthFunc = D3D11_COMPARISON_ALWAYS;          // 항상 무조건 그리기

    Device->CreateDepthStencilState(&dsDescIgnore, &DepthStateIgnore);
}

void URenderer::ReleaseDepthStencilState()
{
    if (DepthStateDefault)
    {
        DepthStateDefault->Release();
        DepthStateDefault = nullptr;
    }
    if (DepthStateIgnore)
    {
        DepthStateIgnore->Release();
        DepthStateIgnore = nullptr;
    }
}

void URenderer::SetDepthStencilEnable(bool bEnable)
{
    if (DeviceContext == nullptr)
        return;

    if (bEnable)
    {
        DeviceContext->OMSetDepthStencilState(DepthStateDefault, 0);
        DeviceContext->RSSetViewports(1, &ViewportInfo);
    }
    else
    {
        DeviceContext->OMSetDepthStencilState(DepthStateIgnore, 0);
    }
}

void URenderer::SetCullMode(ECullMode Mode)
{
    CurrentCullMode = Mode;
    ApplyRasterizerState();
}

void URenderer::SetViewMode(EViewModeIndex Mode)
{
    ViewModeIndex = Mode;
    ApplyRasterizerState();
}

void URenderer::SetShowFlags(EEngineShowFlags InShowFlags)
{
    ShowFlags = InShowFlags;
}

void URenderer::ApplyRasterizerState()
{
    ApplyRasterizerState(CurrentCullMode, false);
}

void URenderer::ApplyRasterizerState(ECullMode CullMode, bool bIgnoreWireframe)
{
    if (DeviceContext == nullptr)
        return;

    // 와이어프레임 상태가 켜져 있더라도 특정 렌더 경로는 솔리드로 유지한다.
    if (ViewModeIndex == EViewModeIndex::VMI_Wireframe && !bIgnoreWireframe)
    {
        DeviceContext->RSSetState(RasterizerStateWireframe);
        return;
    }

    switch (CullMode)
    {
    case ECullMode::None:
        DeviceContext->RSSetState(RasterizerStateCullNone);
        break;
    case ECullMode::Front:
        DeviceContext->RSSetState(RasterizerStateCullFront);
        break;
    case ECullMode::Back:
    default:
        DeviceContext->RSSetState(RasterizerStateCullBack);
        break;
    }
}

void URenderer::CreateBlendState()
{
    D3D11_BLEND_DESC BlendDesc = {};
    BlendDesc.RenderTarget[0].BlendEnable = TRUE;
    BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    Device->CreateBlendState(&BlendDesc, &BlendState);
}

void URenderer::ReleaseBlendState()
{
    if (BlendState)
    {
        BlendState->Release();
        BlendState = nullptr;
    }
}

void URenderer::Prepare(const FSceneViewOptions& ViewOptions)
{
    DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);
    DeviceContext->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    DeviceContext->RSSetViewports(1, &ViewportInfo);

    ShowFlags = ViewOptions.ShowFlags;
    ViewModeIndex = ViewOptions.ViewMode;

    ApplyRasterizerState();

    DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, DepthStencilView);
    DeviceContext->OMSetBlendState(BlendState, BlendFactor, 0xffffffff);
    DeviceContext->OMSetDepthStencilState(DepthStateDefault, 0);

    PrepareShader();

    if (DepthStencilView == nullptr)
    {
        DeviceContext->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
    }
}

void URenderer::PrepareShader()
{
    DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
    DeviceContext->PSSetShader(SimplePixelShader, nullptr, 0);
    DeviceContext->IASetInputLayout(SimpleInputLayout);

    if (ConstantBuffer)
    {
        DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
    }
    if (ConstantBufferColor)
    {
        DeviceContext->VSSetConstantBuffers(1, 1, &ConstantBufferColor);
        DeviceContext->PSSetConstantBuffers(1, 1, &ConstantBufferColor);
    }
}

void URenderer::EnsureTextBuffers(FTextGpuBuffer& Buffers, uint32 RequiredVBSize, uint32 RequiredIBSize)
{
    if (!Buffers.VertexBuffer || Buffers.VertexBufferSize < RequiredVBSize)
    {
        Buffers.VertexBuffer.Reset();

        D3D11_BUFFER_DESC VBDesc = {};
        VBDesc.Usage = D3D11_USAGE_DYNAMIC;
        VBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        VBDesc.ByteWidth = RequiredVBSize;
        VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        if (SUCCEEDED(Device->CreateBuffer(&VBDesc, nullptr, Buffers.VertexBuffer.GetAddressOf())))
        {
            Buffers.VertexBufferSize = RequiredVBSize;
        }
    }

    if (!Buffers.IndexBuffer || Buffers.IndexBufferSize < RequiredIBSize)
    {
        Buffers.IndexBuffer.Reset();

        D3D11_BUFFER_DESC IBDesc = {};
        IBDesc.Usage = D3D11_USAGE_DYNAMIC;
        IBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        IBDesc.ByteWidth = RequiredIBSize;
        IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

        if (SUCCEEDED(Device->CreateBuffer(&IBDesc, nullptr, Buffers.IndexBuffer.GetAddressOf())))
        {
            Buffers.IndexBufferSize = RequiredIBSize;
        }
    }
}

void URenderer::UploadTextBuffers(FTextGpuBuffer& Buffers, const TArray<FTextureVertex>& Vertices,
                                  const TArray<uint32>& Indices)
{
    if (!Buffers.VertexBuffer || !Buffers.IndexBuffer)
        return;

    const UINT VBSize = static_cast<UINT>(sizeof(FTextureVertex) * Vertices.size());
    const UINT IBSize = static_cast<UINT>(sizeof(uint32) * Indices.size());

    D3D11_MAPPED_SUBRESOURCE mapped = {};

    if (SUCCEEDED(DeviceContext->Map(Buffers.VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, Vertices.data(), VBSize);
        DeviceContext->Unmap(Buffers.VertexBuffer.Get(), 0);
    }
    else
    {
        return;
    }

    if (SUCCEEDED(DeviceContext->Map(Buffers.IndexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, Indices.data(), IBSize);
        DeviceContext->Unmap(Buffers.IndexBuffer.Get(), 0);
    }
}

void URenderer::DrawTextBuffers(const FString& FontPath, const FConstants& Constants, const FTextGpuBuffer& Buffers,
                                uint32 IndexCount)
{
    if (!Buffers.VertexBuffer || !Buffers.IndexBuffer || IndexCount == 0)
        return;

    ID3D11ShaderResourceView* SRV = UTextureManager::Get().GetTexture(FontPath);
    if (!SRV)
        return;

    UpdateConstant(Constants);

    UINT Stride = sizeof(FTextureVertex);
    UINT Offset = 0;
    ID3D11Buffer* VB = Buffers.VertexBuffer.Get();
    ID3D11Buffer* IB = Buffers.IndexBuffer.Get();

    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    DeviceContext->IASetVertexBuffers(0, 1, &VB, &Stride, &Offset);
    DeviceContext->IASetIndexBuffer(IB, DXGI_FORMAT_R32_UINT, 0);
    DeviceContext->IASetInputLayout(TextInputLayout);

    ApplyRasterizerState(ECullMode::None, true);

    DeviceContext->VSSetShader(TextVertexShader, nullptr, 0);
    DeviceContext->PSSetShader(TextPixelShader, nullptr, 0);
    DeviceContext->PSSetShaderResources(0, 1, &SRV);
    DeviceContext->PSSetSamplers(0, 1, &LinearSamplerState);

    DeviceContext->DrawIndexed(IndexCount, 0, 0);
}

void URenderer::RenderTextBatch(const FString& FontPath, const FConstants& Constants,
                                const TArray<FTextureVertex>& Vertices, const TArray<uint32>& Indices,
                                FTextGpuBuffer& Buffers)
{
    if (Vertices.empty() || Indices.empty())
        return;

    const uint32 RequiredVBSize = static_cast<uint32>(sizeof(FTextureVertex) * Vertices.size());
    const uint32 RequiredIBSize = static_cast<uint32>(sizeof(uint32) * Indices.size());

    EnsureTextBuffers(Buffers, RequiredVBSize, RequiredIBSize);
    UploadTextBuffers(Buffers, Vertices, Indices);
    DrawTextBuffers(FontPath, Constants, Buffers, static_cast<uint32>(Indices.size()));
}

void URenderer::UndoRenderText()
{

    ID3D11ShaderResourceView* NullSRV = nullptr;
    DeviceContext->PSSetShaderResources(0, 1, &NullSRV);

    DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
    DeviceContext->PSSetShader(SimplePixelShader, nullptr, 0);
    DeviceContext->IASetInputLayout(SimpleInputLayout);
    ApplyRasterizerState();
}

void URenderer::RenderPrimitive(UPrimitiveComponent* primitive, FConstants& constants, FConstantsColor& constantsColor)
{
    // 1. 전달받은 상수 데이터(constants)를 GPU의 Constant Buffer에 업데이트
    UpdateConstant(constants);
    UpdateConstant(constantsColor);

    // 2. 컴포넌트가 무슨 타입(Cube, Axis 등)인지 확인하고 MeshManager에서 실제 GPU 버퍼 조회
    EPrimitiveType Type = primitive->GetPrimitiveType();

    ID3D11Buffer* VertexBuffer = UMeshManager::Get().GetVertexBuffer(Type);
    uint32 NumVertices = UMeshManager::Get().GetNumVertices(Type);

    ID3D11Buffer* IndexBuffer = UMeshManager::Get().GetIndexBuffer(Type);
    uint32 NumIndices = UMeshManager::Get().GetNumIndices(Type);

    // 3. 위상(Topology) 설정
    DeviceContext->IASetPrimitiveTopology(primitive->GetTopology());

    uint32 offset = 0;

    uint32 Stride = UMeshManager::Get().GetStride(Type); 
    DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &offset);

    if (IndexBuffer)
    {
        DeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R16_UINT, 0);
        DeviceContext->DrawIndexed(NumIndices, 0, 0);
    }
    else
    {
        DeviceContext->Draw(NumVertices, 0);
    }
}

ID3D11Buffer* URenderer::CreateVertexBuffer(const FTextureVertex* vertices, uint32 byteWidth)
{
    D3D11_BUFFER_DESC vertexbufferdesc = {};
    vertexbufferdesc.ByteWidth = byteWidth;
    vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE; // will never be updated
    vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexbufferSRD = {vertices};

    ID3D11Buffer* vertexBuffer;

    Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);

    return vertexBuffer;
}

ID3D11Buffer* URenderer::CreateVertexBuffer(const FVertex* vertices, uint32 byteWidth)
{
    // Create a vertex buffer
    D3D11_BUFFER_DESC vertexbufferdesc = {};
    vertexbufferdesc.ByteWidth = byteWidth;
    vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE; // will never be updated
    vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexbufferSRD = {vertices};

    ID3D11Buffer* vertexBuffer;

    Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);

    return vertexBuffer;
}

void URenderer::ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer)
{
    vertexBuffer->Release();
}

ID3D11Buffer* URenderer::CreateTextureVertexBuffer(const FTextureVertex* vertices, uint32 byteWidth)
{
    // Create a vertex buffer
    D3D11_BUFFER_DESC vertexbufferdesc = {};
    vertexbufferdesc.ByteWidth = byteWidth;
    vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE; // will never be updated
    vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexbufferSRD = {vertices};

    ID3D11Buffer* vertexBuffer;

    Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);

    return vertexBuffer;
}

void URenderer::ReleaseTextureVertexBuffer(ID3D11Buffer* vertexBuffer)
{
    vertexBuffer->Release();
}

ID3D11Buffer* URenderer::CreateIndexBuffer(const uint16* indices, uint32 byteWidth)
{
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = byteWidth;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA srd = {indices};

    ID3D11Buffer* buffer;
    Device->CreateBuffer(&desc, &srd, &buffer);

    return buffer;
}

void URenderer::ReleaseIndexBuffer(ID3D11Buffer* indexBuffer)
{
    if (indexBuffer)
        indexBuffer->Release();
}

ID3D11Buffer* URenderer::CreateDynamicVertexBuffer(uint32 byteWidth)
{
    D3D11_BUFFER_DESC vertexbufferdesc = {};
    vertexbufferdesc.ByteWidth = byteWidth;
    vertexbufferdesc.Usage = D3D11_USAGE_DYNAMIC;
    vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ID3D11Buffer* buffer = nullptr;
    Device->CreateBuffer(&vertexbufferdesc, nullptr, &buffer);
    return buffer;
}

void URenderer::ReleaseDynamicVertexBuffer(ID3D11Buffer* vertexBuffer)
{
    if (vertexBuffer)
    {
        vertexBuffer->Release();
    }
}

ID3D11Buffer* URenderer::CreateDynamicIndexBuffer(uint32 byteWidth)
{
    D3D11_BUFFER_DESC indexbufferdesc = {};
    indexbufferdesc.ByteWidth = byteWidth;
    indexbufferdesc.Usage = D3D11_USAGE_DYNAMIC;
    indexbufferdesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ID3D11Buffer* buffer = nullptr;
    Device->CreateBuffer(&indexbufferdesc, nullptr, &buffer);
    return buffer;
}

void URenderer::ReleaseDynamicIndexBuffer(ID3D11Buffer* indexBuffer)
{
    if (indexBuffer)
    {
        indexBuffer->Release();
    }
}

void URenderer::UpdateDynamicBuffer(ID3D11Buffer* Buffer, const void* Data, uint32 byteWidth)
{
    if (!Buffer || !Data || byteWidth == 0)
    {
        return;
    }

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    if (SUCCEEDED(DeviceContext->Map(Buffer, 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
    {
        memcpy(mappedResource.pData, Data, byteWidth);
        DeviceContext->Unmap(Buffer, 0);
    }
}

void URenderer::CreateConstantBuffer()
{
    D3D11_BUFFER_DESC constantbufferdesc = {};
    constantbufferdesc.ByteWidth = sizeof(FConstants) + 0xf & 0xfffffff0;
    constantbufferdesc.Usage = D3D11_USAGE_DYNAMIC;
    constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    constantbufferdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    Device->CreateBuffer(&constantbufferdesc, nullptr, &ConstantBuffer);

    D3D11_BUFFER_DESC constantbuffercolordesc = {};
    constantbuffercolordesc.ByteWidth = sizeof(FConstantsColor) + 0xf & 0xfffffff0;
    constantbuffercolordesc.Usage = D3D11_USAGE_DYNAMIC;
    constantbuffercolordesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    constantbuffercolordesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    Device->CreateBuffer(&constantbuffercolordesc, nullptr, &ConstantBufferColor);
}

void URenderer::ReleaseConstantBuffer()
{
    if (ConstantBuffer)
    {
        ConstantBuffer->Release();
        ConstantBuffer = nullptr;
    }

    if (ConstantBufferColor)
    {
        ConstantBufferColor->Release();
        ConstantBufferColor = nullptr;
    }
}

void URenderer::UpdateConstant(FConstants data)
{
    if (ConstantBuffer)
    {
        D3D11_MAPPED_SUBRESOURCE constantbufferMSR;

        DeviceContext->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0,
                           &constantbufferMSR); // update constant buffer every frame
        FConstants* constants = (FConstants*)constantbufferMSR.pData;

        if (Viewport != nullptr)
        {
            FMatrix<float> viewMatrix = Viewport->GetViewportClient()->GetViewMatrix();
            FMatrix<float> projectionMatrix =
                Viewport->GetViewportClient()->GetProjectionMatrix(ViewportInfo.Width, ViewportInfo.Height);

            constants->MVPMatrix = data.MVPMatrix * viewMatrix * projectionMatrix;
        }

        DeviceContext->Unmap(ConstantBuffer, 0);
    }
}

void URenderer::UpdateConstant(FConstantsColor data)
{
    if (ConstantBufferColor)
    {
        D3D11_MAPPED_SUBRESOURCE constantbufferMSR;

        DeviceContext->Map(ConstantBufferColor, 0, D3D11_MAP_WRITE_DISCARD, 0,
                           &constantbufferMSR); // update constant buffer every frame
        FConstantsColor* constants = (FConstantsColor*)constantbufferMSR.pData;

        *constants = data;

        DeviceContext->Unmap(ConstantBufferColor, 0);
    }
}

bool URenderer::GetCameraBasis(FVector<float>& OutRight, FVector<float>& OutUp, FVector<float>& OutForward) const
{

    // 유효성 검사
    if (Viewport == nullptr || Viewport->GetViewportClient() == nullptr)
    {
        return false;
    }

    // 현재 카메라의 뷰 변환 행렬을 가져옵니다.
    FMatrix<float> ViewMatrix = Viewport->GetViewportClient()->GetViewMatrix();

    // DirectX 기반의 행 우선(Row-Major) 뷰 행렬 기준,
    // 역행렬(World 행렬)의 회전 성분은 뷰 행렬의 전치(Transpose)된 형태로 들어있습니다.
    // 따라서 각 열(Column)의 데이터를 읽어오면 카메라의 기저 벡터를 얻을 수 있습니다.
    OutRight = FVector<float>(ViewMatrix.M[0][0], ViewMatrix.M[1][0], ViewMatrix.M[2][0]);
    OutUp = FVector<float>(ViewMatrix.M[0][1], ViewMatrix.M[1][1], ViewMatrix.M[2][1]);
    OutForward = FVector<float>(ViewMatrix.M[0][2], ViewMatrix.M[1][2], ViewMatrix.M[2][2]);

    return true;
}

void URenderer::OnResize(uint32 NewWidth, uint32 NewHeight)
{
    OutputDebugStringA(("OnResize: " + std::to_string(NewWidth) + "x" + std::to_string(NewHeight) + "\n").c_str());
    if (!SwapChain)
        return;

    // 기존 RTV 해제 (SwapChain 참조 끊기)
    DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

    ReleaseFrameBuffer();
    ReleaseDepthStencilBuffer();

    // SwapChain 버퍼 리사이즈
    HRESULT hr = SwapChain->ResizeBuffers(0, NewWidth, NewHeight, DXGI_FORMAT_UNKNOWN, 0);

    // RTV 재생성
    SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);

    D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
    framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    Device->CreateRenderTargetView(FrameBuffer, &framebufferRTVdesc, &FrameBufferRTV);

    // Depth Buffer 재생성
    CreateDepthStencilBuffer(NewWidth, NewHeight);

    // Viewport 재설정
    ViewportInfo = {0.f, 0.f, (float)NewWidth, (float)NewHeight, 0.f, 1.f};
}

void URenderer::SetTopology(D3D11_PRIMITIVE_TOPOLOGY InTopology)
{
    Topology = InTopology;
    DeviceContext->IASetPrimitiveTopology(Topology);
}

void URenderer::RenderScene(FScene* Scene)
{
    if (!Scene || !DeviceContext)
        return;

    // Primitive 렌더링을 껐다면 메시 그리기를 생략한다.
    if (!CheckShowFlag(EEngineShowFlags::SF_Primitives))
        return;

    // 씬에 등록된 모든 렌더 프록시 순회
    const TArray<FRenderProxy*>& Proxies = Scene->GetProxies();

    // 이전에 최적화했던 Visible 프록시 수집 및 정렬 로직
    TArray<FRenderProxy*> RenderableProxies;
    RenderableProxies.reserve(Proxies.size());

    for (FRenderProxy* Proxy : Proxies)
    {
        if (Proxy && Proxy->RenderCommand.bIsVisible)
        {
            RenderableProxies.push_back(Proxy);
        }
    }

    // Vertex Buffer를 기준으로 정렬한다. (Vertex Buffer 바인딩 비용을 아낄 수 있다.)
    std::sort(RenderableProxies.begin(), RenderableProxies.end(), [](const FRenderProxy* A, const FRenderProxy* B) {
        return A->RenderCommand.VertexBuffer < B->RenderCommand.VertexBuffer;
    });

    // 상태 변경 최소화를 위한 이전 Vertex Buffer 캐싱 변수
    ID3D11Buffer* LastVertexBuffer = nullptr;
    bool bLastTextured = false;

    for (FRenderProxy* Proxy : RenderableProxies)
    {
        if (!Proxy || !Proxy->RenderCommand.bIsVisible)
            continue;

        const FRenderCommand& Command = Proxy->RenderCommand;

        // 1. 이전 프록시와 다른 Vertex Buffer를 사용할 때만 렌더 스테이트 및 버퍼를 새로 바인딩
        if (LastVertexBuffer != Command.VertexBuffer)
        {
            // 렌더 스테이트 설정 (Depth, Cull, Topology)
            SetDepthStencilEnable(Command.bEnableDepthTest);
            CurrentCullMode = Command.CullMode;
            ApplyRasterizerState(Command.CullMode, Command.bIgnoreWireframe);
            SetTopology(Command.Topology);

            // Vertex Buffer 바인딩
            if (Command.VertexBuffer)
            {
                uint32 offset = 0;
                DeviceContext->IASetVertexBuffers(0, 1, &Command.VertexBuffer, &Command.Stride, &offset);
            }
            if (Command.IndexBuffer)
            {
                DeviceContext->IASetIndexBuffer(Command.IndexBuffer, DXGI_FORMAT_R16_UINT, 0);
            }
            if (Command.bIsTextured != bLastTextured)
            {
                if (!Command.bIsTextured)
                {
                    DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
                    DeviceContext->PSSetShader(SimplePixelShader, nullptr, 0);
                    DeviceContext->IASetInputLayout(SimpleInputLayout);
                }
                else
                {
                    DeviceContext->VSSetShader(TextVertexShader, nullptr, 0);
                    DeviceContext->PSSetShader(TextPixelShader, nullptr, 0);
                    DeviceContext->IASetInputLayout(TextInputLayout);
                }
                bLastTextured = Command.bIsTextured;
            }

            // 텍스처가 적용된 프록시라면, 각 컴포넌트가 지정한 텍스처(SRV)를 바인딩
            if (Command.bIsTextured && Command.TextureSRV != nullptr)
            {
                DeviceContext->PSSetShaderResources(0, 1, &Command.TextureSRV);
            }

            // 캐시 업데이트
            LastVertexBuffer = Command.VertexBuffer;
            UpdateConstant(Command.ConstantsColor);
        }

        // 2. 상수 버퍼(Constant Buffer) 갱신
        UpdateConstant(Command.Constants);
        UpdateConstant(Command.ConstantsColor);

        // 3. 그리기 (Draw Call)
        if (Command.IndexBuffer)
        {
            DeviceContext->DrawIndexed(Command.NumIndices, 0, 0);
        }
        else if (Command.VertexBuffer) // 인덱스 버퍼가 없는 경우
        {
            uint32 NumVertices = Command.NumVertices;
            uint32 index = 0;

            // SubUV 컴포넌트인 경우 현재 프레임에 따라 그려준다.
            if (Command.CurrentFrame != -1)
            {
                NumVertices = 6;
                index = Command.CurrentFrame * 6;
            }
            DeviceContext->Draw(NumVertices, index);
        }
    }
}
