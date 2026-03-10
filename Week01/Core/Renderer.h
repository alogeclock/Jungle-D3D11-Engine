#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include "../Gameplay/Camera2D.h"
#include "Texture.h"
#include "Types.h"

class FRenderer
{
public:
	int ScreenWidth = 0;
	int ScreenHeight = 0;

	void* RenderTargetView = nullptr;

public:
	bool Initialize(void* windowHandle, int screenWidth, int screenHeight);
	void Shutdown();

	void BeginFrame();
	void EndFrame();

	void DrawTexture(const FTexture* texture, float screenX, float screenY, float width,
		float height);

	void DrawTextureInWorld(const FTexture* texture, float worldX, float worldY, float width,
		float height, const FCamera2D& camera);

	int GetScreenWidth() const;
	int GetScreenHeight() const;


public:
	struct FConstants
	{
		FVector Offset;
		float Scale;
		float Angle;
		float ChargeSign;
		float Pad[2];
	};

	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;

	ID3D11Texture2D* FrameBuffer = nullptr;
	ID3D11RenderTargetView* FrameBufferRTV = nullptr;
	ID3D11RasterizerState* RasterizerState = nullptr;
	ID3D11Buffer* ConstantBuffer = nullptr;

	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
	D3D11_VIEWPORT ViewportInfo;

	ID3D11VertexShader* SimpleVertexShader;
	ID3D11PixelShader* SimplePixelShader;
	ID3D11InputLayout* SimpleInputLayout;
	unsigned int Stride;

	void UpdateConstant(FVector Offset, float Scale = 1.0f, float Angle = 0.0f, float ChargeSign = 0.0f);
	void Prepare();
	void PrepareShader();
	void RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices);
	void CreateShader();
	void ReleaseShader();

	void Create(HWND hWindow);
	void CreateDeviceAndSwapChain(HWND hWindow);
	void ReleaseDeviceAndSwapChain();
	void CreateFrameBuffer();
	void ReleaseFrameBuffer();
	void CreateRasterizerState();
	void ReleaseRasterizerState();
	void Release();
	void SwapBuffer();

	ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth);
	void ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer);
	void CreateConstantBuffer();
	void ReleaseConstantBuffer();
};