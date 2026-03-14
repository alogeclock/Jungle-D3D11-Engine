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
#include "Engine/Source/Runtime/Core/Public/CoreTypes.h"
//#include "Engine/Source/Runtime/Editor/Public/Viewport.h"

class FViewport;

struct FVertexSimple
{
	float x, y, z;
	float r, g, b, a;
};

struct FConstants
{
	FMatrix<float> worldMatrix;
	FMatrix<float> viewMatrix;
	FMatrix<float> projectionMatrix;
};

class URenderer
{
public:	
	URenderer();
	~URenderer();

public:
	ID3D11Device* Device = nullptr; // GPU와 통신하기 위한 DirectX3D 장치를 의미한다.
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;

	ID3D11Texture2D* FrameBuffer = nullptr;
	ID3D11RenderTargetView* FrameBufferRTV = nullptr; // texture를 렌더 타겟으로 사용하는 view
	ID3D11RasterizerState* RasterizerState = nullptr; // 래스터라이저 상태(cull, fill mode 등을 정의)
	ID3D11Buffer* ConstantBuffer = nullptr; // shader에 데이터를 전송하기 위한 상수 버퍼

	// 깊이 스텐실 상태 객체
    ID3D11DepthStencilState* DepthState_Default = nullptr; // 일반적인 3D 렌더링용 (Depth 켬)
    ID3D11DepthStencilState* DepthState_Ignore = nullptr;  // 기즈모, UI용 (Depth 끔)

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

	void CreateDepthStencilState();
	void ReleaseDepthStencilState();
	void SetDepthTestEnable(bool bEnable);

	void Prepare();
	void PrepareShader();

	void RenderPrimitive(ID3D11Buffer* pBuffer, uint32 numVertices);
	void RenderPrimitiveTopology(ID3D11Buffer* pBuffer, uint32 numVertices, D3D11_PRIMITIVE_TOPOLOGY inTopology);

	ID3D11Buffer* CreateVertexBuffer(const FVertex *vertices, uint32 byteWidth);
	void ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer);

	void CreateConstantBuffer();
	void ReleaseConstantBuffer();

	void UpdateConstant(FConstants data);

	void SetViewport(FViewport* viewport) { Viewport = viewport; };

private:
	FViewport* Viewport = nullptr;
};
