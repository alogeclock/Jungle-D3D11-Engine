#include "DepthOfFieldPass.h"
#include "RenderPassRegistry.h"

#include "Render/Device/D3DDevice.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Shader/ShaderManager.h"

REGISTER_RENDER_PASS(FDepthOfFieldPass)
#include "Render/Types/FrameContext.h"
#include "Render/Types/RenderConstants.h"
#include "Render/Command/DrawCommandList.h"

namespace
{
	constexpr float DOF_SENSOR_WIDTH_MM = 36.0f;
	constexpr float DOF_DEPTH_TO_MILLIMETERS = 1000.0f;
	constexpr uint32 DOF_MAX_BLUR_RADIUS = 12;

	struct FDepthOfFieldConstants
	{
		float FocalLength;
		float FNumber;
		float FocusDistance;
		float FocusRange;
		float MaxCoCRadius;
		float MaxNearCoCRadius;
		float MaxFarCoCRadius;
		float SensorWidth;
		float DepthToMillimeters;
		uint32 DebugView;
		uint32 BlurRadius;
		float NearCoCScale;
	};
	static_assert(sizeof(FDepthOfFieldConstants) % 16 == 0);

	struct FDepthOfFieldBlurConstants
	{
		float BlurDirection[2];
		uint32 BlurRadius;
		float Pad;
	};
	static_assert(sizeof(FDepthOfFieldBlurConstants) % 16 == 0);
}

FDepthOfFieldPass::FDepthOfFieldPass()
{
	PassType = ERenderPass::DepthOfField;
	RenderState = { EDepthStencilState::NoDepth, EBlendState::Opaque,
		ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}

FDepthOfFieldPass::~FDepthOfFieldPass()
{
	ReleaseResources();
}

void FDepthOfFieldPass::FDOFRenderTarget::Release()
{
	if (SRV) { SRV->Release(); SRV = nullptr; }
	if (RTV) { RTV->Release(); RTV = nullptr; }
	if (Texture) { Texture->Release(); Texture = nullptr; }
}

bool FDepthOfFieldPass::BeginPass(const FPassContext& Ctx)
{
	const FFrameContext& Frame = Ctx.Frame;
	if (!Frame.CameraDepth.bEnableDOF || !Frame.RenderOptions.ShowFlags.bDepthOfField)
		return false;

	if (!Frame.ViewportRenderTexture || !Frame.SceneColorCopyTexture || !Frame.SceneColorCopySRV ||
		!Frame.DepthTexture || !Frame.DepthCopyTexture || !Frame.DepthCopySRV)
	{
		return false;
	}

	if (!EnsureResources(Ctx))
	{
		return false;
	}

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	FStateCache& Cache = Ctx.Cache;

	ID3D11ShaderResourceView* NullSRV = nullptr;
	DC->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &NullSRV);
	DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &NullSRV);

	DC->OMSetRenderTargets(0, nullptr, nullptr);
	DC->CopyResource(Frame.SceneColorCopyTexture, Frame.ViewportRenderTexture);
	DC->CopyResource(Frame.DepthCopyTexture, Frame.DepthTexture);
	DC->OMSetRenderTargets(1, &Cache.RTV, Cache.DSV);

	ID3D11ShaderResourceView* DepthSRV = Frame.DepthCopySRV;
	ID3D11ShaderResourceView* SceneColorSRV = Frame.SceneColorCopySRV;
	DC->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &DepthSRV);
	DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &SceneColorSRV);

	Cache.bForceAll = true;
	return true;
}

void FDepthOfFieldPass::EndPass(const FPassContext& Ctx)
{
	ID3D11ShaderResourceView* NullSRV = nullptr;
	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	UnbindLocalSRVs(DC);
	DC->PSSetShaderResources(ESystemTexSlot::SceneDepth, 1, &NullSRV);
	DC->PSSetShaderResources(ESystemTexSlot::SceneColor, 1, &NullSRV);
}

void FDepthOfFieldPass::Execute(const FPassContext& Ctx)
{
	if (!FullCoC.IsValid() ||
		!HalfNearA.IsValid() || !HalfNearB.IsValid() ||
		!HalfFarA.IsValid() || !HalfFarB.IsValid())
	{
		return;
	}

	FShader* CoCShader = FShaderManager::Get().GetOrCreate(EShaderPath::DepthOfFieldCoC);
	FShader* PrefilterShader = FShaderManager::Get().GetOrCreate(EShaderPath::DepthOfFieldPrefilter);
	FShader* BlurShader = FShaderManager::Get().GetOrCreate(EShaderPath::DepthOfFieldBlur);
	FShader* CompositeShader = FShaderManager::Get().GetOrCreate(EShaderPath::DepthOfFieldComposite);
	if (!CoCShader || !CoCShader->IsValid() ||
		!PrefilterShader || !PrefilterShader->IsValid() ||
		!BlurShader || !BlurShader->IsValid() ||
		!CompositeShader || !CompositeShader->IsValid())
	{
		return;
	}

	Ctx.Resources.SetDepthStencilState(Ctx.Device, EDepthStencilState::NoDepth);
	Ctx.Resources.SetBlendState(Ctx.Device, EBlendState::Opaque);
	Ctx.Resources.SetRasterizerState(Ctx.Device, ERasterizerState::SolidNoCull);

	UpdateDOFConstants(Ctx);
	RenderCoCPass(Ctx, CoCShader);
	RenderPrefilterSplitPass(Ctx, PrefilterShader);
	FDOFRenderTarget* FarResult = RenderBlurPingPong(Ctx, BlurShader, HalfFarA, HalfFarB);
	FDOFRenderTarget* NearResult = RenderBlurPingPong(Ctx, BlurShader, HalfNearA, HalfNearB);
	RenderCompositePass(Ctx, CompositeShader, FarResult, NearResult);

	Ctx.Cache.bForceAll = true;
}

bool FDepthOfFieldPass::EnsureResources(const FPassContext& Ctx)
{
	const FFrameContext& Frame = Ctx.Frame;
	const uint32 Width = static_cast<uint32>(Frame.ViewportWidth > 1.0f ? Frame.ViewportWidth : 1.0f);
	const uint32 Height = static_cast<uint32>(Frame.ViewportHeight > 1.0f ? Frame.ViewportHeight : 1.0f);
	const uint32 HalfWidth = (Width + 1) / 2;
	const uint32 HalfHeight = (Height + 1) / 2;

	const bool bSizeMatches =
		ResourceWidth == Width &&
		ResourceHeight == Height &&
		HalfResourceWidth == HalfWidth &&
		HalfResourceHeight == HalfHeight;

	if (bSizeMatches && FullCoC.IsValid() && HalfNearA.IsValid() && HalfNearB.IsValid() &&
		HalfFarA.IsValid() && HalfFarB.IsValid())
	{
		return true;
	}

	ReleaseResources();

	ID3D11Device* Device = Ctx.Device.GetDevice();
	if (!Device)
	{
		return false;
	}

	if (!DOFCB.GetBuffer())
	{
		DOFCB.Create(Device, sizeof(FDepthOfFieldConstants), "DepthOfFieldCB");
	}
	if (!DOFBlurCB.GetBuffer())
	{
		DOFBlurCB.Create(Device, sizeof(FDepthOfFieldBlurConstants), "DepthOfFieldBlurCB");
	}
	if (!DOFCB.GetBuffer() || !DOFBlurCB.GetBuffer())
	{
		return false;
	}

	ResourceWidth = Width;
	ResourceHeight = Height;
	HalfResourceWidth = HalfWidth;
	HalfResourceHeight = HalfHeight;

	if (!CreateRenderTarget(Device, FullCoC, ResourceWidth, ResourceHeight, DXGI_FORMAT_R16_FLOAT) ||
		!CreateRenderTarget(Device, HalfNearA, HalfResourceWidth, HalfResourceHeight, DXGI_FORMAT_R16G16B16A16_FLOAT) ||
		!CreateRenderTarget(Device, HalfNearB, HalfResourceWidth, HalfResourceHeight, DXGI_FORMAT_R16G16B16A16_FLOAT) ||
		!CreateRenderTarget(Device, HalfFarA, HalfResourceWidth, HalfResourceHeight, DXGI_FORMAT_R16G16B16A16_FLOAT) ||
		!CreateRenderTarget(Device, HalfFarB, HalfResourceWidth, HalfResourceHeight, DXGI_FORMAT_R16G16B16A16_FLOAT))
	{
		ReleaseResources();
		return false;
	}

	return true;
}

bool FDepthOfFieldPass::CreateRenderTarget(ID3D11Device* Device, FDOFRenderTarget& Target, uint32 Width, uint32 Height, DXGI_FORMAT Format)
{
	Target.Release();

	D3D11_TEXTURE2D_DESC TextureDesc = {};
	TextureDesc.Width = Width;
	TextureDesc.Height = Height;
	TextureDesc.MipLevels = 1;
	TextureDesc.ArraySize = 1;
	TextureDesc.Format = Format;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.Usage = D3D11_USAGE_DEFAULT;
	TextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(Device->CreateTexture2D(&TextureDesc, nullptr, &Target.Texture)))
	{
		Target.Release();
		return false;
	}

	if (FAILED(Device->CreateRenderTargetView(Target.Texture, nullptr, &Target.RTV)))
	{
		Target.Release();
		return false;
	}

	if (FAILED(Device->CreateShaderResourceView(Target.Texture, nullptr, &Target.SRV)))
	{
		Target.Release();
		return false;
	}

	return true;
}

void FDepthOfFieldPass::ReleaseResources()
{
	FullCoC.Release();
	HalfNearA.Release();
	HalfNearB.Release();
	HalfFarA.Release();
	HalfFarB.Release();

	ResourceWidth = 0;
	ResourceHeight = 0;
	HalfResourceWidth = 0;
	HalfResourceHeight = 0;
}

void FDepthOfFieldPass::UnbindLocalSRVs(ID3D11DeviceContext* DC)
{
	ID3D11ShaderResourceView* NullSRVs[3] = {};
	DC->PSSetShaderResources(0, 3, NullSRVs);
}

void FDepthOfFieldPass::SetViewport(ID3D11DeviceContext* DC, uint32 Width, uint32 Height)
{
	D3D11_VIEWPORT Viewport = {};
	Viewport.TopLeftX = 0.0f;
	Viewport.TopLeftY = 0.0f;
	Viewport.Width = static_cast<float>(Width);
	Viewport.Height = static_cast<float>(Height);
	Viewport.MinDepth = 0.0f;
	Viewport.MaxDepth = 1.0f;
	DC->RSSetViewports(1, &Viewport);
}

void FDepthOfFieldPass::DrawFullscreen(ID3D11DeviceContext* DC)
{
	ID3D11Buffer* NullBuffers[2] = {};
	uint32 Strides[2] = {};
	uint32 Offsets[2] = {};
	DC->IASetVertexBuffers(0, 2, NullBuffers, Strides, Offsets);
	DC->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
	DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DC->Draw(3, 0);
}

void FDepthOfFieldPass::UpdateDOFConstants(const FPassContext& Ctx)
{
	const FCameraDepthViewData& CameraDepth = Ctx.Frame.CameraDepth;
	const float MaxCoC = CameraDepth.MaxCoCRadius > 0.0f ? CameraDepth.MaxCoCRadius : 0.0f;
	const uint32 BlurRadius = static_cast<uint32>(MaxCoC > static_cast<float>(DOF_MAX_BLUR_RADIUS) ? DOF_MAX_BLUR_RADIUS : MaxCoC);

	FDepthOfFieldConstants Data = {};
	Data.FocalLength = CameraDepth.FocalLength;
	Data.FNumber = CameraDepth.Aperture;
	Data.FocusDistance = CameraDepth.CurrentFocusDistance;
	Data.FocusRange = CameraDepth.FocusRange;
	Data.MaxCoCRadius = CameraDepth.MaxCoCRadius;
	Data.MaxNearCoCRadius = CameraDepth.MaxNearCoCRadius;
	Data.MaxFarCoCRadius = CameraDepth.MaxFarCoCRadius;
	Data.SensorWidth = DOF_SENSOR_WIDTH_MM;
	Data.DepthToMillimeters = DOF_DEPTH_TO_MILLIMETERS;
	Data.DebugView = static_cast<uint32>(Ctx.Frame.RenderOptions.DepthOfFieldDebugView);
	Data.BlurRadius = BlurRadius;
	Data.NearCoCScale = CameraDepth.NearCoCScale;
	DOFCB.Update(Ctx.Device.GetDeviceContext(), &Data, sizeof(Data));

	ID3D11Buffer* RawCB = DOFCB.GetBuffer();
	Ctx.Device.GetDeviceContext()->VSSetConstantBuffers(ECBSlot::PerShader0, 1, &RawCB);
	Ctx.Device.GetDeviceContext()->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &RawCB);
}

void FDepthOfFieldPass::UpdateBlurConstants(const FPassContext& Ctx, float DirectionX, float DirectionY)
{
	const FCameraDepthViewData& CameraDepth = Ctx.Frame.CameraDepth;
	const float MaxCoC = CameraDepth.MaxCoCRadius > 0.0f ? CameraDepth.MaxCoCRadius : 0.0f;
	const uint32 BlurRadius = static_cast<uint32>(MaxCoC > static_cast<float>(DOF_MAX_BLUR_RADIUS) ? DOF_MAX_BLUR_RADIUS : MaxCoC);

	FDepthOfFieldBlurConstants Data = {};
	Data.BlurDirection[0] = DirectionX;
	Data.BlurDirection[1] = DirectionY;
	Data.BlurRadius = BlurRadius;
	DOFBlurCB.Update(Ctx.Device.GetDeviceContext(), &Data, sizeof(Data));

	ID3D11Buffer* RawCB = DOFBlurCB.GetBuffer();
	Ctx.Device.GetDeviceContext()->VSSetConstantBuffers(ECBSlot::PerShader0, 1, &RawCB);
	Ctx.Device.GetDeviceContext()->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &RawCB);
}

void FDepthOfFieldPass::RenderCoCPass(const FPassContext& Ctx, FShader* Shader)
{
	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	UnbindLocalSRVs(DC);
	SetViewport(DC, ResourceWidth, ResourceHeight);
	DC->OMSetRenderTargets(1, &FullCoC.RTV, nullptr);
	Shader->Bind(DC);
	DrawFullscreen(DC);
}

void FDepthOfFieldPass::RenderPrefilterSplitPass(const FPassContext& Ctx, FShader* Shader)
{
	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	UnbindLocalSRVs(DC);
	SetViewport(DC, HalfResourceWidth, HalfResourceHeight);

	ID3D11RenderTargetView* RTVs[2] = { HalfNearA.RTV, HalfFarA.RTV };
	DC->OMSetRenderTargets(2, RTVs, nullptr);

	ID3D11ShaderResourceView* FullCoCSRV = FullCoC.SRV;
	DC->PSSetShaderResources(0, 1, &FullCoCSRV);
	Shader->Bind(DC);
	DrawFullscreen(DC);
}

FDepthOfFieldPass::FDOFRenderTarget* FDepthOfFieldPass::RenderBlurPingPong(const FPassContext& Ctx, FShader* Shader, FDOFRenderTarget& LayerA, FDOFRenderTarget& LayerB)
{
	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	SetViewport(DC, HalfResourceWidth, HalfResourceHeight);
	Shader->Bind(DC);

	UnbindLocalSRVs(DC);
	DC->OMSetRenderTargets(1, &LayerB.RTV, nullptr);
	ID3D11ShaderResourceView* SourceSRV = LayerA.SRV;
	DC->PSSetShaderResources(0, 1, &SourceSRV);
	UpdateBlurConstants(Ctx, 1.0f, 0.0f);
	DrawFullscreen(DC);

	UnbindLocalSRVs(DC);
	DC->OMSetRenderTargets(1, &LayerA.RTV, nullptr);
	SourceSRV = LayerB.SRV;
	DC->PSSetShaderResources(0, 1, &SourceSRV);
	UpdateBlurConstants(Ctx, 0.0f, 1.0f);
	DrawFullscreen(DC);

	return &LayerA;
}

void FDepthOfFieldPass::RenderCompositePass(const FPassContext& Ctx, FShader* Shader, FDOFRenderTarget* FarResult, FDOFRenderTarget* NearResult)
{
	if (!FarResult || !NearResult)
	{
		return;
	}

	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	UnbindLocalSRVs(DC);
	SetViewport(DC, ResourceWidth, ResourceHeight);
	DC->OMSetRenderTargets(1, &Ctx.Cache.RTV, nullptr);

	ID3D11ShaderResourceView* SRVs[3] = { FarResult->SRV, NearResult->SRV, FullCoC.SRV };
	DC->PSSetShaderResources(0, 3, SRVs);

	ID3D11Buffer* RawCB = DOFCB.GetBuffer();
	DC->VSSetConstantBuffers(ECBSlot::PerShader0, 1, &RawCB);
	DC->PSSetConstantBuffers(ECBSlot::PerShader0, 1, &RawCB);

	Shader->Bind(DC);
	DrawFullscreen(DC);
}
