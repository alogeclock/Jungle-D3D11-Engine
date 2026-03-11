#include "pch.h"
#include "Renderer.h"
#include "FontManager.h"
#include "Texture.h"
#include "Gameplay/SpriteInfo.h"
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;
using namespace DirectX;

void FRenderer::Prepare()
{
	DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	DeviceContext->RSSetViewports(1, &ViewportInfo);
	DeviceContext->RSSetState(RasterizerState);

	DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, nullptr);
	DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
}

void FRenderer::BindShader(EShaderType Type)
{
	switch (Type)
	{
	case EShaderType::Default:
		DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
		DeviceContext->PSSetShader(SimplePixelShader, nullptr, 0);
		DeviceContext->IASetInputLayout(SimpleInputLayout);
		if (ConstantBuffer)
		{
			DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
			DeviceContext->PSSetConstantBuffers(0, 1, &ConstantBuffer);
		}
		break;

	case EShaderType::UI:
		DeviceContext->VSSetShader(FontVertexShader, nullptr, 0);
		DeviceContext->PSSetShader(FontPixelShader, nullptr, 0);
		DeviceContext->IASetInputLayout(FontInputLayout);
		if (FontConstantBuffer)
		{
			DeviceContext->VSSetConstantBuffers(0, 1, &FontConstantBuffer);
			DeviceContext->PSSetConstantBuffers(0, 1, &FontConstantBuffer);
		}
		break;
	}
}

void FRenderer::PrepareShader()
{
	BindShader(EShaderType::Default);
}

void FRenderer::SetCamera(const FCamera2D& Camera)
{
	FVec2 CamPos = Camera.GetPosition();
	float Zoom = Camera.GetZoom();
	float VpW = ViewportInfo.Width;
	float VpH = ViewportInfo.Height;

	XMMATRIX View = XMMatrixTranslation(-CamPos.X, -CamPos.Y, 0.0f)
		* XMMatrixScaling(Zoom, Zoom, 1.0f);
	XMMATRIX Proj = XMMatrixOrthographicOffCenterLH(0.0f, VpW, VpH, 0.0f, 0.0f, 1.0f);

	XMStoreFloat4x4(&CachedView, XMMatrixTranspose(View));
	XMStoreFloat4x4(&CachedProjection, XMMatrixTranspose(Proj));
}

void FRenderer::DrawSprite(const FTexture* Texture, float WorldX, float WorldY,
	float Width, float Height, const FSpriteInfo& Sprite)
{
	FRenderObject Obj;
	Obj.Texture = Texture;

	XMMATRIX World = XMMatrixScaling(Width, Height, 1.0f)
		* XMMatrixTranslation(WorldX, WorldY, 0.0f);
	XMStoreFloat4x4(&Obj.World, XMMatrixTranspose(World));

	Obj.SpriteOffset = Sprite.SpriteOffset;
	Obj.IsMirrored = Sprite.bIsMirrored ? 1.0f : 0.0f;

	if (Texture)
	{
		XMFLOAT2 TexSize = { static_cast<float>(Texture->Width), static_cast<float>(Texture->Height) };
		Obj.TextureSize = TexSize;
		// 아틀라스 미사용 시 전체 텍스처 UV 사용 (SpriteSize == TextureSize → 비율 1.0)
		Obj.SpriteSize = TexSize;
	}

	Obj.bScreenSpace = false;
	RenderObjects.push_back(Obj);
}

void FRenderer::DrawTexture(const FTexture* texture, float screenX, float screenY,
	float width, float height)
{
	FRenderObject Obj;
	Obj.Texture = texture;

	XMMATRIX World = XMMatrixScaling(width, height, 1.0f)
		* XMMatrixTranslation(screenX, screenY, 0.0f);
	XMStoreFloat4x4(&Obj.World, XMMatrixTranspose(World));

	if (texture)
	{
		Obj.TextureSize = { static_cast<float>(texture->Width), static_cast<float>(texture->Height) };
		Obj.SpriteSize = Obj.TextureSize;
	}

	Obj.bScreenSpace = true;
	RenderObjects.push_back(Obj);
}

FStaticBatch FRenderer::CreateStaticBatch(const FVertexSimple* Vertices, UINT VertexCount,
	const UINT* Indices, UINT IndexCount)
{
	FStaticBatch Batch;

	D3D11_BUFFER_DESC vbDesc = {};
	vbDesc.ByteWidth = sizeof(FVertexSimple) * VertexCount;
	vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vbData = { Vertices };
	Device->CreateBuffer(&vbDesc, &vbData, &Batch.VertexBuffer);

	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.ByteWidth = sizeof(UINT) * IndexCount;
	ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA ibData = { Indices };
	Device->CreateBuffer(&ibDesc, &ibData, &Batch.IndexBuffer);

	Batch.IndexCount = IndexCount;
	return Batch;
}

void FRenderer::ReleaseStaticBatch(FStaticBatch& Batch)
{
	if (Batch.VertexBuffer) { Batch.VertexBuffer->Release(); Batch.VertexBuffer = nullptr; }
	if (Batch.IndexBuffer) { Batch.IndexBuffer->Release(); Batch.IndexBuffer = nullptr; }
	Batch.IndexCount = 0;
}

void FRenderer::DrawBatch(const FStaticBatch& Batch, const FTexture* Texture)
{
	if (!Batch.VertexBuffer || !Batch.IndexBuffer || Batch.IndexCount == 0)
		return;

	PrepareShader();

	UINT stride = Stride;
	UINT offset = 0;
	DeviceContext->IASetVertexBuffers(0, 1, &Batch.VertexBuffer, &stride, &offset);
	DeviceContext->IASetIndexBuffer(Batch.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	DeviceContext->PSSetSamplers(0, 1, &SamplerState);

	if (Texture && Texture->GetTextureSRV())
	{
		ID3D11ShaderResourceView* SRV = Texture->GetTextureSRV();
		DeviceContext->PSSetShaderResources(0, 1, &SRV);
	}

	// World = identity, 정점에 월드 좌표가 이미 bake 됨
	FSpriteConstants CB = {};
	XMStoreFloat4x4(&CB.World, XMMatrixTranspose(XMMatrixIdentity()));
	CB.View = CachedView;
	CB.Projection = CachedProjection;
	if (Texture)
	{
		XMFLOAT2 TexSize = { (float)Texture->Width, (float)Texture->Height };
		CB.SpriteSize = TexSize;
		CB.TextureSize = TexSize;
	}

	D3D11_MAPPED_SUBRESOURCE Mapped;
	DeviceContext->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
	memcpy(Mapped.pData, &CB, sizeof(CB));
	DeviceContext->Unmap(ConstantBuffer, 0);

	DeviceContext->DrawIndexed(Batch.IndexCount, 0, 0);
}

void FRenderer::Render()
{
	if (RenderObjects.empty() && FontRenderObjects.empty())
		return;

	PrepareShader();

	UINT offset = 0;
	DeviceContext->IASetVertexBuffers(0, 1, &QuadBuffer, &Stride, &offset);
	DeviceContext->PSSetSamplers(0, 1, &SamplerState);

	// 스크린 스페이스용 View/Projection
	XMFLOAT4X4 ScreenView, ScreenProjection;
	XMStoreFloat4x4(&ScreenView, XMMatrixTranspose(XMMatrixIdentity()));
	XMStoreFloat4x4(&ScreenProjection, XMMatrixTranspose(
		XMMatrixOrthographicOffCenterLH(0.0f, (float)ScreenWidth, (float)ScreenHeight, 0.0f, 0.0f, 1.0f)));

	for (const FRenderObject& Obj : RenderObjects)
	{
		if (Obj.Texture && Obj.Texture->GetTextureSRV())
		{
			ID3D11ShaderResourceView* SRV = Obj.Texture->GetTextureSRV();
			DeviceContext->PSSetShaderResources(0, 1, &SRV);
		}

		FSpriteConstants CB = {};
		CB.World = Obj.World;
		CB.View = Obj.bScreenSpace ? ScreenView : CachedView;
		CB.Projection = Obj.bScreenSpace ? ScreenProjection : CachedProjection;
		CB.SpriteSize = Obj.SpriteSize;
		CB.TextureSize = Obj.TextureSize;
		CB.SpriteOffset = Obj.SpriteOffset;
		CB.IsMirrored = Obj.IsMirrored;
		CB.Pad = 0.0f;

		D3D11_MAPPED_SUBRESOURCE Mapped;
		DeviceContext->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
		memcpy(Mapped.pData, &CB, sizeof(CB));
		DeviceContext->Unmap(ConstantBuffer, 0);

		DeviceContext->Draw(6, 0);
	}

	// Font rendering (UI shader)
	if (!FontRenderObjects.empty())
	{
		BindShader(EShaderType::UI);

		for (const FFontRenderObject& RenderObject : FontRenderObjects)
		{
			if (RenderObject.Texture && RenderObject.Texture->GetTextureSRV())
			{
				ID3D11ShaderResourceView* SRV = RenderObject.Texture->GetTextureSRV();
				DeviceContext->PSSetShaderResources(0, 1, &SRV);
			}

			FVector Offset(RenderObject.Offset.X, RenderObject.Offset.Y, 0.f);
			UpdateFontConstant(Offset, RenderObject.Size.X, RenderObject.Size.Y);

			FVec2 UV0 = RenderObject.UV0;
			FVec2 UV1 = RenderObject.UV1;

			FVertexSimple textVertices[6] =
			{
				{ -0.5f, -0.5f, 0.f, UV0.X, UV0.Y }, // 좌하단
				{  0.5f, -0.5f, 0.f, UV1.X, UV0.Y }, // 우하단
				{  0.5f,  0.5f, 0.f, UV1.X, UV1.Y }, // 우상단

				{ -0.5f, -0.5f, 0.f, UV0.X, UV0.Y }, // 좌하단
				{  0.5f,  0.5f, 0.f, UV1.X, UV1.Y }, // 우상단
				{ -0.5f,  0.5f, 0.f, UV0.X, UV1.Y }, // 좌상단
			};

			UpdateTextQuadBuffer(textVertices);
			UINT fontOffset = 0;
			DeviceContext->IASetVertexBuffers(0, 1, &TextQuadBuffer, &Stride, &fontOffset);
			DeviceContext->Draw(6, 0);
		}
	}
}

bool FRenderer::LoadShaderFromFile(const std::wstring& Path)
{
	ShaderError.clear();

	ID3DBlob* VsBlob = nullptr;
	ID3DBlob* PsBlob = nullptr;
	ID3DBlob* ErrorBlob = nullptr;

	HRESULT Hr = D3DCompileFromFile(Path.c_str(), nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &VsBlob, &ErrorBlob);
	if (FAILED(Hr))
	{
		if (ErrorBlob)
		{
			ShaderError = "VS: " + std::string((const char*)ErrorBlob->GetBufferPointer(), ErrorBlob->GetBufferSize());
			ErrorBlob->Release();
		}
		else
		{
			ShaderError = "VS: Failed to open file";
		}
		if (VsBlob) VsBlob->Release();
		return false;
	}

	Hr = D3DCompileFromFile(Path.c_str(), nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &PsBlob, &ErrorBlob);
	if (FAILED(Hr))
	{
		if (ErrorBlob)
		{
			ShaderError = "PS: " + std::string((const char*)ErrorBlob->GetBufferPointer(), ErrorBlob->GetBufferSize());
			ErrorBlob->Release();
		}
		else
		{
			ShaderError = "PS: Failed to compile";
		}
		if (PsBlob) PsBlob->Release();
		VsBlob->Release();
		return false;
	}

	ID3D11VertexShader* NewVS = nullptr;
	ID3D11PixelShader* NewPS = nullptr;
	ID3D11InputLayout* NewIL = nullptr;

	Device->CreateVertexShader(VsBlob->GetBufferPointer(), VsBlob->GetBufferSize(), nullptr, &NewVS);
	Device->CreatePixelShader(PsBlob->GetBufferPointer(), PsBlob->GetBufferSize(), nullptr, &NewPS);

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	Device->CreateInputLayout(layout, ARRAYSIZE(layout), VsBlob->GetBufferPointer(), VsBlob->GetBufferSize(), &NewIL);

	VsBlob->Release();
	PsBlob->Release();

	if (!NewVS || !NewPS || !NewIL)
	{
		ShaderError = "Failed to create shader objects";
		if (NewVS) NewVS->Release();
		if (NewPS) NewPS->Release();
		if (NewIL) NewIL->Release();
		return false;
	}

	ReleaseShader();
	SimpleVertexShader = NewVS;
	SimplePixelShader = NewPS;
	SimpleInputLayout = NewIL;

	// 파일명 추출하여 현재 셰이더 이름 저장
	fs::path FsPath(Path);
	CurrentShaderName = FsPath.stem().string();

	return true;
}

const std::string& FRenderer::GetShaderError() const
{
	return ShaderError;
}

std::vector<std::string> FRenderer::GetAvailableShaders() const
{
	std::vector<std::string> Names;
	const std::string ShaderDir = "Resources/Shaders";
	if (fs::exists(ShaderDir) && fs::is_directory(ShaderDir))
	{
		for (const auto& Entry : fs::directory_iterator(ShaderDir))
		{
			if (Entry.path().extension() == ".hlsl")
			{
				Names.push_back(Entry.path().stem().string());
			}
		}
	}
	return Names;
}

const std::string& FRenderer::GetCurrentShaderName() const
{
	return CurrentShaderName;
}

void FRenderer::CreateShader()
{
	ID3DBlob* vertexshaderCSO;
	ID3DBlob* pixelshaderCSO;

	D3DCompileFromFile(L"Resources/Shaders/Default.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &vertexshaderCSO, nullptr);

	Device->CreateVertexShader(vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), nullptr, &SimpleVertexShader);

	D3DCompileFromFile(L"Resources/Shaders/Default.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &pixelshaderCSO, nullptr);

	Device->CreatePixelShader(pixelshaderCSO->GetBufferPointer(), pixelshaderCSO->GetBufferSize(), nullptr, &SimplePixelShader);

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	Device->CreateInputLayout(layout, ARRAYSIZE(layout), vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), &SimpleInputLayout);

	Stride = sizeof(FVertexSimple);

	vertexshaderCSO->Release();
	pixelshaderCSO->Release();
}

void FRenderer::ReleaseShader()
{
	if (SimpleInputLayout)
	{
		SimpleInputLayout->Release();
		SimpleInputLayout = nullptr;
	}

	if (SimplePixelShader)
	{
		SimplePixelShader->Release();
		SimplePixelShader = nullptr;
	}

	if (SimpleVertexShader)
	{
		SimpleVertexShader->Release();
		SimpleVertexShader = nullptr;
	}
}

void FRenderer::CreateFontShader()
{
	ID3DBlob* VsBlob = nullptr;
	ID3DBlob* PsBlob = nullptr;

	D3DCompileFromFile(L"Resources/Shaders/UI.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &VsBlob, nullptr);
	Device->CreateVertexShader(VsBlob->GetBufferPointer(), VsBlob->GetBufferSize(), nullptr, &FontVertexShader);

	D3DCompileFromFile(L"Resources/Shaders/UI.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &PsBlob, nullptr);
	Device->CreatePixelShader(PsBlob->GetBufferPointer(), PsBlob->GetBufferSize(), nullptr, &FontPixelShader);

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	Device->CreateInputLayout(layout, ARRAYSIZE(layout), VsBlob->GetBufferPointer(), VsBlob->GetBufferSize(), &FontInputLayout);

	VsBlob->Release();
	PsBlob->Release();
}

void FRenderer::ReleaseFontShader()
{
	if (FontInputLayout) { FontInputLayout->Release(); FontInputLayout = nullptr; }
	if (FontPixelShader) { FontPixelShader->Release(); FontPixelShader = nullptr; }
	if (FontVertexShader) { FontVertexShader->Release(); FontVertexShader = nullptr; }
	if (FontConstantBuffer) { FontConstantBuffer->Release(); FontConstantBuffer = nullptr; }
}

void FRenderer::CreateFontConstantBuffer()
{
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = sizeof(FConstants) + 0xf & 0xfffffff0;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	Device->CreateBuffer(&desc, nullptr, &FontConstantBuffer);
}

void FRenderer::UpdateFontConstant(FVector Offset, float ScaleX, float ScaleY)
{
	if (FontConstantBuffer)
	{
		D3D11_MAPPED_SUBRESOURCE mapped;
		DeviceContext->Map(FontConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		FConstants* constants = (FConstants*)mapped.pData;
		{
			constants->Offset = Offset;
			constants->ScaleX = ScaleX;
			constants->ScaleY = ScaleY;
			constants->ScreenSize = { (float)ScreenWidth, (float)ScreenHeight };
		}
		DeviceContext->Unmap(FontConstantBuffer, 0);
	}
}

void FRenderer::Create(HWND hWindow)
{
	CreateDeviceAndSwapChain(hWindow);
	CreateFrameBuffer();
	CreateRasterizerState();
	CreateSimpleQuad();
	CreateTextQuadBuffer();
	CreateSamplerState();
	CreateBlendState();
}

void FRenderer::CreateDeviceAndSwapChain(HWND hWindow)
{
	D3D_FEATURE_LEVEL featurelevels[] = { D3D_FEATURE_LEVEL_11_0 };

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
		D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
		featurelevels, ARRAYSIZE(featurelevels), D3D11_SDK_VERSION,
		&swapchaindesc, &SwapChain, &Device, nullptr, &DeviceContext);

	SwapChain->GetDesc(&swapchaindesc);

	ScreenWidth = swapchaindesc.BufferDesc.Width;
	ScreenHeight = swapchaindesc.BufferDesc.Height;

	ViewportInfo = { 0.0f, 0.0f, (float)ScreenWidth, (float)ScreenHeight, 0.0f, 1.0f };
}

void FRenderer::ReleaseDeviceAndSwapChain()
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

void FRenderer::CreateFrameBuffer()
{
	SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);

	D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
	framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

	Device->CreateRenderTargetView(FrameBuffer, &framebufferRTVdesc, &FrameBufferRTV);
}

void FRenderer::ReleaseFrameBuffer()
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

void FRenderer::CreateRasterizerState()
{
	D3D11_RASTERIZER_DESC rasterizerdesc = {};
	rasterizerdesc.FillMode = D3D11_FILL_SOLID;
	rasterizerdesc.CullMode = D3D11_CULL_BACK;

	Device->CreateRasterizerState(&rasterizerdesc, &RasterizerState);
}

void FRenderer::ReleaseRasterizerState()
{
	if (RasterizerState)
	{
		RasterizerState->Release();
		RasterizerState = nullptr;
	}
}

void FRenderer::Release()
{
	ReleaseFontShader();
	if (AlphaBlendState) { AlphaBlendState->Release(); AlphaBlendState = nullptr; }
	RasterizerState->Release();

	DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

	ReleaseFrameBuffer();
	ReleaseDeviceAndSwapChain();
}

void FRenderer::SwapBuffer()
{
	SwapChain->Present(1, 0);
}

void FRenderer::CreateSamplerState()
{
	D3D11_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;

	samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;

	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	Device->CreateSamplerState(&samplerDesc, &SamplerState);
}

void FRenderer::CreateBlendState()
{
	D3D11_BLEND_DESC Desc = {};
	Desc.RenderTarget[0].BlendEnable = TRUE;
	Desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	Desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	Desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	Desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	Desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	Desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	Desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	Device->CreateBlendState(&Desc, &AlphaBlendState);
}

void FRenderer::DrawDarknessOverlay(const FTexture* Texture, float ScreenCenterX, float ScreenCenterY)
{
	if (!Texture || !Texture->GetTextureSRV()) return;

	// 알파 블렌딩 활성화
	DeviceContext->OMSetBlendState(AlphaBlendState, nullptr, 0xffffffff);

	PrepareShader();

	UINT offset = 0;
	DeviceContext->IASetVertexBuffers(0, 1, &QuadBuffer, &Stride, &offset);
	DeviceContext->PSSetSamplers(0, 1, &SamplerState);

	ID3D11ShaderResourceView* SRV = Texture->GetTextureSRV();
	DeviceContext->PSSetShaderResources(0, 1, &SRV);

	// 플레이어 화면 좌표를 중심으로, 화면 전체를 덮을 크기의 쿼드 (2배)
	float W = ViewportInfo.Width;
	float H = ViewportInfo.Height;
	float QW = W * 2.0f;
	float QH = H * 2.0f;

	FSpriteConstants CB = {};
	XMMATRIX World = XMMatrixScaling(QW, QH, 1.0f)
		* XMMatrixTranslation(ScreenCenterX, ScreenCenterY, 0.0f);
	XMStoreFloat4x4(&CB.World, XMMatrixTranspose(World));
	XMStoreFloat4x4(&CB.View, XMMatrixTranspose(XMMatrixIdentity()));
	XMStoreFloat4x4(&CB.Projection, XMMatrixTranspose(
		XMMatrixOrthographicOffCenterLH(0.0f, W, H, 0.0f, 0.0f, 1.0f)));
	XMFLOAT2 TexSize = { (float)Texture->Width, (float)Texture->Height };
	CB.SpriteSize = TexSize;
	CB.TextureSize = TexSize;

	D3D11_MAPPED_SUBRESOURCE Mapped;
	DeviceContext->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
	memcpy(Mapped.pData, &CB, sizeof(CB));
	DeviceContext->Unmap(ConstantBuffer, 0);

	DeviceContext->Draw(6, 0);

	// 블렌딩 해제
	DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
}

void FRenderer::CreateSimpleQuad()
{
	D3D11_BUFFER_DESC bufferDesc = {};
	bufferDesc.ByteWidth = sizeof(quadVertices);
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = quadVertices;

	Device->CreateBuffer(
		&bufferDesc,
		&initData,
		&QuadBuffer
	);
}

ID3D11Buffer* FRenderer::CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth)
{
	D3D11_BUFFER_DESC vertexbufferdesc = {};
	vertexbufferdesc.ByteWidth = byteWidth;
	vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexbufferSRD = { vertices };

	ID3D11Buffer* vertexBuffer;

	Device->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);

	return vertexBuffer;
}

void FRenderer::CreateTextQuadBuffer()
{
	D3D11_BUFFER_DESC vertexbufferdesc = {};
	vertexbufferdesc.ByteWidth = sizeof(quadVertices);
	vertexbufferdesc.Usage = D3D11_USAGE_DYNAMIC;
	vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	Device->CreateBuffer(&vertexbufferdesc, nullptr, &TextQuadBuffer);
}

void FRenderer::UpdateTextQuadBuffer(FVertexSimple* textVertices)
{
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	DeviceContext->Map(TextQuadBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	memcpy(mapped.pData, textVertices, sizeof(FVertexSimple) * 6);
	DeviceContext->Unmap(TextQuadBuffer, 0);
}

void FRenderer::ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer)
{
	vertexBuffer->Release();
}

void FRenderer::CreateConstantBuffer()
{
	D3D11_BUFFER_DESC constantbufferdesc = {};
	constantbufferdesc.ByteWidth = (sizeof(FSpriteConstants) + 0xf) & 0xfffffff0;
	constantbufferdesc.Usage = D3D11_USAGE_DYNAMIC;
	constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	constantbufferdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	Device->CreateBuffer(&constantbufferdesc, nullptr, &ConstantBuffer);
}

void FRenderer::ReleaseConstantBuffer()
{
	if (ConstantBuffer)
	{
		ConstantBuffer->Release();
		ConstantBuffer = nullptr;
	}
}

void FRenderer::DrawFont(const std::string& text, const FBitmapFont* Font, const FTexture* texture,
	float screenX, float screenY, float scale)
{
	float ScaleFactor = 1.0f;
	if (Font->LineHeight > 0)
	{
		ScaleFactor = scale / static_cast<float>(Font->LineHeight);
	}

	std::vector<FTextQuad> Quads = BuildQuads(*Font, text, screenX, screenY, ScaleFactor);
	FFontRenderObject FontRenderObj;
	for (const FTextQuad& Quad : Quads)
	{
		FontRenderObj.Offset.X = Quad.ScreenX;
		FontRenderObj.Offset.Y = Quad.ScreenY;
		FontRenderObj.Size.X = Quad.Width;
		FontRenderObj.Size.Y = Quad.Height;
		FontRenderObj.Texture = texture;
		FontRenderObj.UV0.X = Quad.U0;
		FontRenderObj.UV0.Y = Quad.V0;
		FontRenderObj.UV1.X = Quad.U1;
		FontRenderObj.UV1.Y = Quad.V1;

		FontRenderObjects.push_back(FontRenderObj);
	}
}

bool FRenderer::Initialize(HWND hWindow, int InScreenWidth, int InScreenHeight)
{
	Create(hWindow);
	CreateShader();
	CreateConstantBuffer();
	CreateFontShader();
	CreateFontConstantBuffer();

	// CachedView/Projection 초기화 (기본값: identity / screen ortho)
	XMStoreFloat4x4(&CachedView, XMMatrixTranspose(XMMatrixIdentity()));
	XMStoreFloat4x4(&CachedProjection, XMMatrixTranspose(
		XMMatrixOrthographicOffCenterLH(0.0f, (float)ScreenWidth, (float)ScreenHeight, 0.0f, 0.0f, 1.0f)));

	RenderObjects.reserve(256);

	return Device != nullptr;
}

void FRenderer::BeginFrame()
{
	Prepare();
}

void FRenderer::EndFrame()
{
	SwapBuffer();
	RenderObjects.clear();
	FontRenderObjects.clear();
}

int FRenderer::GetScreenWidth() const
{
	return static_cast<int>(ViewportInfo.Width);
}

int FRenderer::GetScreenHeight() const
{
	return static_cast<int>(ViewportInfo.Height);
}
