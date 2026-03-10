#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <string>
#include <vector>
#include "Gameplay/Camera2D.h"
#include "RenderObject.h"
#include "Core/Types.h"

struct FSpriteInfo;

// Default.hlsl의 SpriteConstants cbuffer와 1:1 매칭
struct FSpriteConstants
{
	DirectX::XMFLOAT4X4 World;
	DirectX::XMFLOAT4X4 View;
	DirectX::XMFLOAT4X4 Projection;
	DirectX::XMFLOAT2   SpriteSize;
	DirectX::XMFLOAT2   TextureSize;
	DirectX::XMFLOAT2   SpriteOffset;
	float               IsMirrored;
	float               Pad;
};

class FRenderer
{
public:
	int ScreenWidth = 0;
	int ScreenHeight = 0;

	void* RenderTargetView = nullptr;

public:
	bool Initialize(HWND windowHandle, int screenWidth, int screenHeight);

	void BeginFrame();
	void EndFrame();

	// 카메라 설정 (월드 스페이스 렌더링용 View/Projection 갱신)
	void SetCamera(const FCamera2D& Camera);

	// 월드 스페이스 스프라이트 큐잉
	void DrawSprite(const FTexture* Texture, float WorldX, float WorldY,
		float Width, float Height, const FSpriteInfo& Sprite);

	// 스크린 스페이스 렌더링 큐잉
	void DrawTexture(const FTexture* texture, float screenX, float screenY,
		float width, float height);

	int GetScreenWidth() const;
	int GetScreenHeight() const;

public:
	bool LoadShaderFromFile(const std::wstring& Path);
	std::vector<std::string> GetAvailableShaders() const;
	const std::string& GetCurrentShaderName() const;
	const std::string& GetShaderError() const;

	void Prepare();
	void PrepareShader();
	void Render();
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
	void CreateSamplerState();
	void CreateSimpleQuad();

	ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth);
	void ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer);
	void CreateConstantBuffer();
	void ReleaseConstantBuffer();

	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;

	ID3D11Texture2D* FrameBuffer = nullptr;
	ID3D11RenderTargetView* FrameBufferRTV = nullptr;
	ID3D11RasterizerState* RasterizerState = nullptr;
	ID3D11Buffer* ConstantBuffer = nullptr;
	ID3D11Buffer* QuadBuffer = nullptr;

	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
	D3D11_VIEWPORT ViewportInfo;

	ID3D11VertexShader* SimpleVertexShader;
	ID3D11PixelShader* SimplePixelShader;
	ID3D11InputLayout* SimpleInputLayout;
	unsigned int Stride;

	ID3D11SamplerState* SamplerState = nullptr;

private:
	std::string CurrentShaderName = "Default";
	std::string ShaderError;
	std::vector<FRenderObject> RenderObjects;

	// 캐시된 View/Projection 행렬 (SetCamera에서 갱신)
	DirectX::XMFLOAT4X4 CachedView;
	DirectX::XMFLOAT4X4 CachedProjection;

	const FVertexSimple quadVertices[6] =
	{
		{ -0.5f, -0.5f, 0.f, 0.f, 0.f },
		{  0.5f, -0.5f, 0.f, 1.f, 0.f },
		{  0.5f,  0.5f, 0.f, 1.f, 1.f },

		{ -0.5f, -0.5f, 0.f, 0.f, 0.f },
		{  0.5f,  0.5f, 0.f, 1.f, 1.f },
		{ -0.5f,  0.5f, 0.f, 0.f, 1.f }
	};
};
