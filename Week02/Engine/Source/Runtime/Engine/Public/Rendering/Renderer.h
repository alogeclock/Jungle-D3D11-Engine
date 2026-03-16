#pragma once

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#include <d3d11.h>
#include <d3dcompiler.h>

#include "Engine/Source/Runtime/Core/Public/Math/Vector.h"
#include "Engine/Source/Runtime/Core/Public/Math/Vector4.h"
#include "Engine/Source/Runtime/Core/Public/Math/Matrix.h"
#include "Engine/Source/Runtime/Core/Public/Math/TranslationMatrix.h"
#include "Engine/Source/Runtime/Core/Public/Math/PerspectiveMatrix.h"
#include "Engine/Source/Runtime/Core/Public/Math/OrthographicMatrix.h"

#include "CoreTypes.h"

class FViewport;
class UMeshManager;
class UPrimitiveComponent;

struct FConstants
{
	FMatrix<float> worldMatrix;
	FMatrix<float> viewMatrix;
	FMatrix<float> projectionMatrix;
};

struct FConstantsColor
{
    float r, g, b, a;
};

class URenderer
{
public:	
	URenderer();
	~URenderer();

public:
	ID3D11Device* Device = nullptr; 
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;

	ID3D11Texture2D* FrameBuffer = nullptr;
    ID3D11Texture2D* DepthStencilBuffer = nullptr;
    ID3D11DepthStencilView* DepthStencilView = nullptr;
	ID3D11RenderTargetView* FrameBufferRTV = nullptr; 

	ID3D11Buffer* ConstantBuffer = nullptr; 
	ID3D11Buffer* ConstantBufferColor = nullptr;

	// 깊이 스텐실 상태 객체
    ID3D11DepthStencilState* DepthStateDefault = nullptr; // 일반적인 3D 렌더링용 (Depth 켬)
    ID3D11DepthStencilState* DepthStateIgnore = nullptr;  // 기즈모, UI용 (Depth 끔)

	ID3D11RasterizerState* RasterizerStateCullBack = nullptr;
    ID3D11RasterizerState* RasterizerStateCullFront = nullptr;
    ID3D11RasterizerState* RasterizerStateCullNone = nullptr;

	ID3D11BlendState* BlendState = nullptr;
    FLOAT BlendFactor[4] = {0.f, 0.f, 0.f, 0.f};

	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f }; // 화면을 초기화하는 색
	D3D11_VIEWPORT ViewportInfo;
	D3D11_PRIMITIVE_TOPOLOGY Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	ID3D11VertexShader* SimpleVertexShader;
	ID3D11PixelShader* SimplePixelShader;
	ID3D11InputLayout* SimpleInputLayout;
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
	void ReleaseShader();

	void CreateDepthStencilBuffer(uint32 width, uint32 height);
    void ReleaseDepthStencilBuffer();

	void CreateDepthStencilState();
	void ReleaseDepthStencilState();
	void SetDepthStencilEnable(bool bEnable);
	void SetCullMode(ECullMode cullMode);

	void CreateBlendState();
    void ReleaseBlendState();

	void Prepare();
	void PrepareShader();

	void RenderPrimitive(ID3D11Buffer* pBuffer, uint32 numVertices);
	void RenderPrimitive(UPrimitiveComponent *primitive);
    void RenderPrimitive(UPrimitiveComponent *primitive, FConstants &constants, FConstantsColor &constantsColor);

	ID3D11Buffer* CreateVertexBuffer(const FVertex *vertices, uint32 byteWidth);
	void ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer);

	void CreateConstantBuffer();
	void ReleaseConstantBuffer();

	void UpdateConstant(FConstants data);
    void UpdateConstant(FConstantsColor data);

	void SetViewport(FViewport* viewport) { Viewport = viewport; };
    void OnResize(uint32 NewWidth, uint32 NewHeight);

private:
	FViewport* Viewport = nullptr;
};
