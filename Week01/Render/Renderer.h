#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include <string>
#include <vector>
#include "Gameplay/Camera2D.h"
#include "RenderObject.h"
#include "Core/Types.h"

class FRenderer
{
public:
	int ScreenWidth = 0;
	int ScreenHeight = 0;

	void* RenderTargetView = nullptr;

public:
	bool Initialize(HWND windowHandle, int screenWidth, int screenHeight);
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
		FVector Offset;    // 12 bytes
		float ScaleX;      // 4 bytes (Total 16)
		FVec2 ScreenSize;  // 8 bytes
		float ScaleY;      // 4 bytes
		float Angle;       // 4 bytes (Total 16)
		float ChargeSign;  // 4 bytes
		float Padding[3];  // 12 bytes (Total 16)
	};

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

	bool LoadShaderFromFile(const std::wstring& Path);
	std::vector<std::string> GetAvailableShaders() const;
	const std::string& GetCurrentShaderName() const;
	const std::string& GetShaderError() const;

	void UpdateConstant(FVector Offset, float ScaleX = 1.0f, float ScaleY = 1.0f, float Angle = 0.0f, float ChargeSign = 0.0f);
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


private:
	std::string CurrentShaderName = "Default";
	std::string ShaderError;
	std::vector<FRenderObject> RenderObjects;
	const FVertexSimple quadVertices[6] =
	{
		//CCW
		//{ -0.5f, -0.5f, 0.f, 0.f, 1.f },//좌하단
		//{  0.5f, -0.5f, 0.f, 1.f, 1.f },//우하단
		//{  0.5f,  0.5f, 0.f, 1.f, 0.f },//우상단

		//{ -0.5f, -0.5f, 0.f, 0.f, 1.f },//좌하단
		//{  0.5f,  0.5f, 0.f, 1.f, 0.f },//우상단
		//{ -0.5f,  0.5f, 0.f, 0.f, 0.f } //좌상단


			{ -0.5f, -0.5f, 0.f, 0.f, 0.f }, // 좌하단
	{  0.5f, -0.5f, 0.f, 1.f, 0.f }, // 우하단
	{  0.5f,  0.5f, 0.f, 1.f, 1.f }, // 우상단

	{ -0.5f, -0.5f, 0.f, 0.f, 0.f }, // 좌하단
	{  0.5f,  0.5f, 0.f, 1.f, 1.f }, // 우상단
	{ -0.5f,  0.5f, 0.f, 0.f, 1.f }  // 좌상단

	//CW
	//{ -0.5f, -0.5f, 0.f, 0.f, 0.f },
	//{  -0.5f, 0.5f, 0.f, 1.f, 0.f },
	//{  0.5f,  0.5f, 0.f, 1.f, 1.f },

	//{ -0.5f, -0.5f, 0.f, 0.f, 0.f },
	//{  0.5f,  0.5f, 0.f, 1.f, 1.f },
	//{  0.5f,  -0.5f, 0.f, 0.f, 1.f }
	};
};