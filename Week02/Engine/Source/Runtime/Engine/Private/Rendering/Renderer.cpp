#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Engine/Public/Rendering/Renderer.h"
#include "Engine/Source/Runtime/Editor/Public/Viewport.h"

URenderer::URenderer() {}

URenderer::~URenderer() {}

void URenderer::Create(HWND hWindow)
{
    CreateDeviceAndSwapChain(hWindow);
    CreateFrameBuffer();
    CreateRasterizerState();

    CreateConstantBuffer();
    CreateDepthStencilBuffer(ViewportInfo.Width, ViewportInfo.Height);
    CreateDepthStencilState();

    CreateShader();
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

    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG, featurelevels,
                                  ARRAYSIZE(featurelevels), D3D11_SDK_VERSION, &swapchaindesc, &SwapChain, &Device, nullptr, &DeviceContext);

    SwapChain->GetDesc(&swapchaindesc);

    ViewportInfo = {0.0f, 0.0f, (float)swapchaindesc.BufferDesc.Width, (float)swapchaindesc.BufferDesc.Height, 0.0f, 1.0f};
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
    SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&FrameBuffer);

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
    SwapChain->Present(1, 0); // 1: Vsync 활성화
}

void URenderer::CreateShader()
{
    ID3DBlob *vertexshaderCSO = nullptr;
    ID3DBlob *pixelshaderCSO = nullptr;

    D3DCompileFromFile(L"ShaderW0.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &vertexshaderCSO, nullptr);

    Device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), nullptr, &SimpleVertexShader);

    D3DCompileFromFile(L"ShaderW0.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &pixelshaderCSO, nullptr);

    Device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(), pixelshaderCSO->GetBufferSize(), nullptr, &SimplePixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    Device->CreateInputLayout(layout, ARRAYSIZE(layout), vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), &SimpleInputLayout);

    Stride = sizeof(FVertex);

    vertexshaderCSO->Release();
    pixelshaderCSO->Release();
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
    }
    else
    {
        DeviceContext->OMSetDepthStencilState(DepthStateIgnore, 0);
    }
}

void URenderer::SetCullMode(ECullMode Mode)
{
    if (DeviceContext == nullptr)
        return;

    switch (Mode)
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

void URenderer::Prepare()
{
    DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);
    DeviceContext->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    DeviceContext->RSSetViewports(1, &ViewportInfo);
    DeviceContext->RSSetState(RasterizerStateCullBack);
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

void URenderer::RenderPrimitive(ID3D11Buffer *pBuffer, uint32 numVertices)
{
    uint32 offset = 0;
    DeviceContext->IASetVertexBuffers(0, 1, &pBuffer, &Stride, &offset);
    DeviceContext->Draw(numVertices, 0);
}

void URenderer::RenderPrimitive(UPrimitiveComponent *primitive)
{
    // 1. 컴포넌트가 무슨 타입(Cube, Axis 등)인지 확인하고 MeshManager에서 실제 GPU 버퍼 조회
    EPrimitiveType Type = primitive->GetPrimitiveType();
    EPrimitiveType Topology = primitive->GetPrimitiveType();
    ID3D11Buffer  *VertexBuffer = UMeshManager::Get().GetVertexBuffer(Type);
    uint32 NumVertices = UMeshManager::Get().GetNumVertices(Type);

    // 2. 위상(Topology) 설정
    DeviceContext->IASetPrimitiveTopology(primitive->GetTopology());
    uint32 offset = 0;
    DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &offset);
    DeviceContext->Draw(NumVertices, 0);
}

void URenderer::RenderPrimitive(UPrimitiveComponent *primitive, FConstants &constants, FConstantsColor &constantsColor)
{
    // [TODO: 상수 버퍼의 World Matrix는 프레임이 시작될 때 1번만 갱신하는 방식으로 최적화할 필요가 있다.]
    
    // 1. 전달받은 상수 데이터(constants)를 GPU의 Constant Buffer에 업데이트
    UpdateConstant(constants);
    UpdateConstant(constantsColor);

    // 2. 컴포넌트가 무슨 타입(Cube, Axis 등)인지 확인하고 MeshManager에서 실제 GPU 버퍼 조회
    EPrimitiveType Type = primitive->GetPrimitiveType();
    ID3D11Buffer  *VertexBuffer = UMeshManager::Get().GetVertexBuffer(Type);
    uint32 NumVertices = UMeshManager::Get().GetNumVertices(Type);

    // 3. 위상(Topology) 설정
    DeviceContext->IASetPrimitiveTopology(primitive->GetTopology());

    uint32 offset = 0;
    DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &offset);
    DeviceContext->Draw(NumVertices, 0);
}

ID3D11Buffer *URenderer::CreateVertexBuffer(const FVertex *vertices, uint32 byteWidth)
{
    // Create a vertex buffer
    D3D11_BUFFER_DESC vertexbufferdesc = {};
    vertexbufferdesc.ByteWidth = byteWidth;
    vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE; // will never be updated
    vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexbufferSRD = {vertices};

    ID3D11Buffer *vertexBuffer;

    Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);

    return vertexBuffer;
}

void URenderer::ReleaseVertexBuffer(ID3D11Buffer *vertexBuffer) { vertexBuffer->Release(); }

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

        DeviceContext->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR); // update constant buffer every frame
        FConstants *constants = (FConstants *)constantbufferMSR.pData;

        if (Viewport != nullptr)
        {
            FMatrix<float> viewMatrix = Viewport->GetViewportClient()->GetViewMatrix();
            FMatrix<float> projectionMatrix = Viewport->GetViewportClient()->GetProjectionMatrix(ViewportInfo.Width, ViewportInfo.Height);

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

        DeviceContext->Map(ConstantBufferColor, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR); // update constant buffer every frame
        FConstantsColor *constants = (FConstantsColor *)constantbufferMSR.pData;

        *constants = data;

        DeviceContext->Unmap(ConstantBufferColor, 0);
    }
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
    SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&FrameBuffer);

    D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
    framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    Device->CreateRenderTargetView(FrameBuffer, &framebufferRTVdesc, &FrameBufferRTV);

    // Depth Buffer 재생성
    CreateDepthStencilBuffer(NewWidth, NewHeight);

    // Viewport 재설정
    ViewportInfo = {0.f, 0.f, (float)NewWidth, (float)NewHeight, 0.f, 1.f};
}
