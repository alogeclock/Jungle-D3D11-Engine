#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <string>
#include <vector>
#include "Gameplay/Camera2D.h"
#include "RenderObject.h"
#include "FontRenderObject.h"
#include "Core/Types.h"
#include "Render/BitmapFont.h"

enum class EShaderType
{
	Default,
	UI,
};

struct FSpriteInfo;

// 정적 배치 (타일, 벽 등 한번에 DrawIndexed)
struct FStaticBatch
{
	ID3D11Buffer* VertexBuffer = nullptr;
	ID3D11Buffer* IndexBuffer = nullptr;
	UINT IndexCount = 0;
};

// 렌더 커맨드 타입 (통합 렌더 큐용)
enum class ERenderCmdType
{
	Batch,
	Sprite,
	DarknessOverlay,
};

struct FBatchRenderCmd
{
	const FStaticBatch* pBatch;
	const FTexture* Texture;
};

struct FDarknessRenderCmd
{
	const FTexture* Texture;
	float ScreenCenterX;
	float ScreenCenterY;
};

struct FRenderCommand
{
	ERenderCmdType Type;
	int Index;
};

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
	DirectX::XMFLOAT4   ColorTint; // RGBA tint (0,0,0,0 = no tint, 1,1,1,1 = white)
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
		float Width, float Height, const FSpriteInfo& Sprite,
		const DirectX::XMFLOAT4& ColorTint = {0.0f, 0.0f, 0.0f, 0.0f});

	// 스크린 스페이스 렌더링 큐잉
	void DrawTexture(const FTexture* texture, float screenX, float screenY,
		float width, float height);
	void DrawTexture(const FTexture* texture, float screenX, float screenY,
		float width, float height, float rotationRad);

	// 암흑 시야 오버레이 (알파 블렌딩으로 전체 화면에 렌더)
	void DrawDarknessOverlay(const FTexture* Texture, float ScreenCenterX, float ScreenCenterY);


	// 정적 배치 생성/해제/렌더
	FStaticBatch CreateStaticBatch(const FVertexSimple* Vertices, UINT VertexCount,
		const UINT* Indices, UINT IndexCount);
	void ReleaseStaticBatch(FStaticBatch& Batch);
	void DrawBatch(const FStaticBatch& Batch, const FTexture* Texture);

	void DrawFont(const std::string& text, const FBitmapFont* Font, const FTexture* texture,
		float screenX, float screenY, float scale);

	int GetScreenWidth() const;
	int GetScreenHeight() const;

public:
	// UI/Font 상수 버퍼용 구조체 (UI.hlsl cbuffer와 1:1 매칭)
	struct FConstants
	{
		FVector Offset;    // 12 bytes
		float ScaleX;      // 4 bytes (Total 16)
		FVec2 ScreenSize;  // 8 bytes
		float ScaleY;      // 4 bytes
		float Padding;     // 4 bytes (Total 16)
	};

	bool LoadShaderFromFile(const std::wstring& Path);
	std::vector<std::string> GetAvailableShaders() const;
	const std::string& GetCurrentShaderName() const;
	const std::string& GetShaderError() const;

	void UpdateFontConstant(FVector Offset, float ScaleX = 1.0f, float ScaleY = 1.0f);
	void BindShader(EShaderType Type);
	void Prepare();
	void PrepareShader();
	void Render();
	void CreateShader();
	void CreateFontShader();
	void ReleaseFontShader();
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
	void CreateBlendState();
	void CreateSimpleQuad();
	void CreateTextQuadBuffer();
	void UpdateTextQuadBuffer(FVertexSimple* textVertices);

	ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth);
	void ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer);
	void CreateConstantBuffer();
	void CreateFontConstantBuffer();
	void ReleaseConstantBuffer();

	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	IDXGISwapChain* SwapChain = nullptr;

	ID3D11Texture2D* FrameBuffer = nullptr;
	ID3D11RenderTargetView* FrameBufferRTV = nullptr;
	ID3D11RasterizerState* RasterizerState = nullptr;
	ID3D11Buffer* ConstantBuffer = nullptr;
	ID3D11Buffer* QuadBuffer = nullptr;
	ID3D11Buffer* TextQuadBuffer = nullptr;

	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
	D3D11_VIEWPORT ViewportInfo;

	ID3D11VertexShader* SimpleVertexShader;
	ID3D11PixelShader* SimplePixelShader;
	ID3D11InputLayout* SimpleInputLayout;
	unsigned int Stride;

	// UI/Font 전용 셰이더 (UI.hlsl)
	ID3D11VertexShader* FontVertexShader = nullptr;
	ID3D11PixelShader* FontPixelShader = nullptr;
	ID3D11InputLayout* FontInputLayout = nullptr;
	ID3D11Buffer* FontConstantBuffer = nullptr;

	ID3D11SamplerState* SamplerState = nullptr;
	ID3D11BlendState* AlphaBlendState = nullptr;

private:
	std::string CurrentShaderName = "Default";
	std::string ShaderError;
	std::vector<FRenderObject> RenderObjects;
	std::vector<FFontRenderObject> FontRenderObjects;

	// 통합 렌더 커맨드 큐
	std::vector<FRenderCommand> RenderCommands;
	std::vector<FBatchRenderCmd> BatchRenderCmds;
	std::vector<FDarknessRenderCmd> DarknessRenderCmds;

	void ExecuteBatch(const FBatchRenderCmd& Cmd);
	void ExecuteSprite(const FRenderObject& Obj);
	void ExecuteDarkness(const FDarknessRenderCmd& Cmd);

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
