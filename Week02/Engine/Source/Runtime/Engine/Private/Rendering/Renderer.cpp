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
    CreateShader();
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
    D3D11_RASTERIZER_DESC rasterizerdesc = {};
    rasterizerdesc.FillMode = D3D11_FILL_SOLID;
    rasterizerdesc.CullMode = D3D11_CULL_BACK; // back-face culling (뒷면을 생략한다.)

    Device->CreateRasterizerState(&rasterizerdesc, &RasterizerState);
}

void URenderer::ReleaseRasterizerState()
{
    if (RasterizerState)
    {
        RasterizerState->Release();
        RasterizerState = nullptr;
    }
}

void URenderer::Release()
{
    ReleaseRasterizerState();

    DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

    ReleaseFrameBuffer();
    ReleaseDeviceAndSwapChain();

    ReleaseConstantBuffer();

    ReleaseShader();
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

void URenderer::CreateDepthStencilState()
{
    // 1. 기본 상태 (깊이 판정 켜기)
    D3D11_DEPTH_STENCIL_DESC dsDescDefault = {};
    dsDescDefault.DepthEnable = TRUE;
    dsDescDefault.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDescDefault.DepthFunc = D3D11_COMPARISON_LESS; // 앞쪽에 있는 픽셀만 그리기

    Device->CreateDepthStencilState(&dsDescDefault, &DepthState_Default);

    // 2. 무시 상태 (깊이 판정 끄기)
    D3D11_DEPTH_STENCIL_DESC dsDescIgnore = {};
    dsDescIgnore.DepthEnable = FALSE;                          // 핵심: Depth 테스트를 아예 수행하지 않음
    dsDescIgnore.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // Depth 버퍼에 쓰기도 방지
    dsDescIgnore.DepthFunc = D3D11_COMPARISON_ALWAYS;          // 항상 무조건 그리기

    Device->CreateDepthStencilState(&dsDescIgnore, &DepthState_Ignore);
}

void URenderer::ReleaseDepthStencilState()
{
    if (DepthState_Default)
    {
        DepthState_Default->Release();
        DepthState_Default = nullptr;
    }
    if (DepthState_Ignore)
    {
        DepthState_Ignore->Release();
        DepthState_Ignore = nullptr;
    }
}

void URenderer::SetDepthTestEnable(bool bEnable)
{
    if (DeviceContext == nullptr)
        return;

    if (bEnable)
    {
        DeviceContext->OMSetDepthStencilState(DepthState_Default, 0);
    }
    else
    {
        DeviceContext->OMSetDepthStencilState(DepthState_Ignore, 0);
    }
}

void URenderer::Prepare()
{
    DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);
    DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    DeviceContext->RSSetViewports(1, &ViewportInfo);
    DeviceContext->RSSetState(RasterizerState);
    DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, nullptr);
    DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);

    PrepareShader();
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
}

void URenderer::RenderPrimitive(ID3D11Buffer *pBuffer, uint32 numVertices)
{
    uint32 offset = 0;
    DeviceContext->IASetVertexBuffers(0, 1, &pBuffer, &Stride, &offset);
    DeviceContext->Draw(numVertices, 0);
}

void URenderer::RenderPrimitive(UPrimitiveComponent *Primitive)
{
    // 1. 컴포넌트가 무슨 타입(Cube, Axis 등)인지 확인하고 MeshManager에서 실제 GPU 버퍼 조회
    EPrimitiveType Type = Primitive->GetPrimitiveType();
    EPrimitiveType Topology = Primitive->GetPrimitiveType();
    ID3D11Buffer* VertexBuffer = MeshManager->GetVertexBuffer(Type);
    uint32 NumVertices = MeshManager->GetNumVertices(Type);

    // 2. 위상(Topology) 설정
    DeviceContext->IASetPrimitiveTopology(Primitive->GetTopology());
    uint32 offset = 0;
    DeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &offset);
    DeviceContext->Draw(NumVertices, 0);
}

void URenderer::RenderPrimitive(UPrimitiveComponent *Primitive, FConstants &constants)
{
    // [TODO: 상수 버퍼의 World Matrix는 프레임이 시작될 때 1번만 갱신하는 방식으로 최적화할 필요가 있다.]
    
    // 1. 전달받은 상수 데이터(constants)를 GPU의 Constant Buffer에 업데이트
    UpdateConstant(constants);

    // 2. 컴포넌트가 무슨 타입(Cube, Axis 등)인지 확인하고 MeshManager에서 실제 GPU 버퍼 조회
    EPrimitiveType Type = Primitive->GetPrimitiveType();
    ID3D11Buffer* VertexBuffer = MeshManager->GetVertexBuffer(Type);
    uint32 NumVertices = MeshManager->GetNumVertices(Type);

    // 3. 위상(Topology) 설정
    DeviceContext->IASetPrimitiveTopology(Primitive->GetTopology());

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

    FConstants constants;
    constants.worldMatrix = FTranslationMatrix<float>::FTranslationMatrix(FVector(1.0f, 0.0f, 0.0f));
    UpdateConstant(constants);
}

void URenderer::ReleaseConstantBuffer()
{
    if (ConstantBuffer)
    {
        ConstantBuffer->Release();
        ConstantBuffer = nullptr;
    }
}

void URenderer::UpdateConstant(FConstants data)
{
    if (ConstantBuffer)
    {
        D3D11_MAPPED_SUBRESOURCE constantbufferMSR;

        DeviceContext->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR); // update constant buffer every frame
        FConstants *constants = (FConstants *)constantbufferMSR.pData;

        constants->worldMatrix = data.worldMatrix;

        if (Viewport != nullptr) {
			constants->viewMatrix = Viewport->GetViewportClient()->GetViewMatrix();
            constants->projectionMatrix = Viewport->GetViewportClient()->GetProjectionMatrix(ViewportInfo.Width, ViewportInfo.Height);
		}

		DeviceContext->Unmap(ConstantBuffer, 0);
	}
}