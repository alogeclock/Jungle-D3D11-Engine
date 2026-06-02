#pragma once

#include "Render/RenderPass/RenderPassBase.h"
#include "Render/Resource/Buffer.h"

class FDepthOfFieldPass : public FRenderPassBase
{
public:
	FDepthOfFieldPass();
	~FDepthOfFieldPass() override;

	// 패스 전후 GPU 상태 전환 (RT 바인딩, 리소스 복사 등)
	virtual bool BeginPass(const FPassContext& Ctx) override;
	virtual void EndPass(const FPassContext& Ctx) override;

	// 패스 실행 — 기본 구현: DrawCommandList에서 패스 범위를 가져와 Submit.
	// ShadowDepth(Cubemap 6면, CSM cascade) 등 커스텀 렌더링이 필요한 패스는 override.
	virtual void Execute(const FPassContext& Ctx) override;

private:
	struct FDOFRenderTarget
	{
		ID3D11Texture2D* Texture = nullptr;
		ID3D11RenderTargetView* RTV = nullptr;
		ID3D11ShaderResourceView* SRV = nullptr;

		bool IsValid() const { return Texture && RTV && SRV; }
		void Release();
	};

	bool EnsureResources(const FPassContext& Ctx);
	bool CreateRenderTarget(ID3D11Device* Device, FDOFRenderTarget& Target, uint32 Width, uint32 Height, DXGI_FORMAT Format);
	void ReleaseResources();
	void UnbindLocalSRVs(ID3D11DeviceContext* DC);
	void SetViewport(ID3D11DeviceContext* DC, uint32 Width, uint32 Height);
	void DrawFullscreen(ID3D11DeviceContext* DC);
	void UpdateDOFConstants(const FPassContext& Ctx);
	void UpdateBlurConstants(const FPassContext& Ctx, float LayerSign);
	void RenderCoCPass(const FPassContext& Ctx, class FShader* Shader);
	void RenderPrefilterSplitPass(const FPassContext& Ctx, class FShader* Shader);
	FDOFRenderTarget* RenderDiskBlur(const FPassContext& Ctx, class FShader* Shader, FDOFRenderTarget& LayerA, FDOFRenderTarget& LayerB, float LayerSign);
	void RenderCompositePass(const FPassContext& Ctx, class FShader* Shader, FDOFRenderTarget* FarResult, FDOFRenderTarget* NearResult);

	uint32 ResourceWidth = 0;
	uint32 ResourceHeight = 0;
	uint32 HalfResourceWidth = 0;
	uint32 HalfResourceHeight = 0;

	FDOFRenderTarget FullCoC;
	FDOFRenderTarget HalfNearA;
	FDOFRenderTarget HalfNearB;
	FDOFRenderTarget HalfFarA;
	FDOFRenderTarget HalfFarB;

	FConstantBuffer DOFCB;
	FConstantBuffer DOFBlurCB;
};
