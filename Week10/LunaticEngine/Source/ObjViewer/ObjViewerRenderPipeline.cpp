#include "ObjViewer/ObjViewerRenderPipeline.h"

#include "ObjViewer/ObjViewerEngine.h"
#include "Render/Pipeline/Renderer.h"
#include "Render/Scene/FScene.h"
#include "Viewport/Viewport.h"
#include "Component/CameraComponent.h"
#include "GameFramework/World.h"
#include "Core/Log.h"
#include "Materials/Material.h"
#include "Render/Proxy/PrimitiveSceneProxy.h"

#include <algorithm>

FObjViewerRenderPipeline::FObjViewerRenderPipeline(UObjViewerEngine* InEngine, FRenderer& InRenderer)
	: Engine(InEngine)
{
}

FObjViewerRenderPipeline::~FObjViewerRenderPipeline()
{
}

static uint32 CountCommandsForPass(const FDrawCommandList& Commands, ERenderPass Pass)
{
	uint32 Count = 0;
	for (const FDrawCommand& Command : Commands.GetCommands())
	{
		if (Command.Pass == Pass)
		{
			++Count;
		}
	}
	return Count;
}

static void ForcePreviewMeshRenderState(FDrawCommandList& Commands)
{
	TArray<FDrawCommand>& MutableCommands = const_cast<TArray<FDrawCommand>&>(Commands.GetCommands());
	MutableCommands.erase(
		std::remove_if(
			MutableCommands.begin(),
			MutableCommands.end(),
			[](const FDrawCommand& Command)
			{
				return Command.Pass == ERenderPass::PreDepth;
			}),
		MutableCommands.end());

	for (FDrawCommand& Command : MutableCommands)
	{
		if (Command.Pass == ERenderPass::Opaque)
		{
			Command.RenderState.DepthStencil = EDepthStencilState::DepthGreaterEqual;
			Command.RenderState.Rasterizer = ERasterizerState::SolidNoCull;
			Command.BuildSortKey();
		}
	}
}

static void LogPreviewProxySections(const FCollectOutput& Output)
{
	uint32 LoggedSections = 0;
	for (FPrimitiveSceneProxy* Proxy : Output.RenderableProxies)
	{
		if (!Proxy)
		{
			continue;
		}

		for (const FMeshSectionDraw& Section : Proxy->GetSectionDraws())
		{
			if (LoggedSections >= 8)
			{
				return;
			}

			UMaterial* Material = Section.Material;
			UE_LOG_CATEGORY(
				FbxImporter,
				Info,
				"ObjViewer preview section[%u]: Material=%p Path=%s Shader=%p MaterialPass=%s FirstIndex=%u IndexCount=%u",
				LoggedSections,
				Material,
				Material ? Material->GetAssetPathFileName().c_str() : "<null>",
				Material ? Material->GetShader() : nullptr,
				Material ? GetRenderPassName(Material->GetRenderPass()) : "<null>",
				Section.FirstIndex,
				Section.IndexCount);

			++LoggedSections;
		}
	}
}

static void LogPreviewDrawCommands(const FDrawCommandList& Commands)
{
	uint32 LoggedCommands = 0;
	uint32 CommandIndex = 0;
	for (const FDrawCommand& Command : Commands.GetCommands())
	{
		if (Command.Pass != ERenderPass::Opaque)
		{
			++CommandIndex;
			continue;
		}

		if (LoggedCommands >= 8)
		{
			return;
		}

		UE_LOG_CATEGORY(
			FbxImporter,
			Info,
			"ObjViewer preview command[%u]: Pass=%s Shader=%p VB=%p IB=%p VBStride=%u FirstIndex=%u IndexCount=%u BaseVertex=%d PerObjectCB=%p PerShader0=%p DiffuseSRV=%p Depth=%d Raster=%d Blend=%d",
			CommandIndex,
			GetRenderPassName(Command.Pass),
			Command.Shader,
			Command.Buffer.VB,
			Command.Buffer.IB,
			Command.Buffer.VBStride,
			Command.Buffer.FirstIndex,
			Command.Buffer.IndexCount,
			Command.Buffer.BaseVertex,
			Command.PerObjectCB,
			Command.Bindings.PerShaderCB[0],
			Command.Bindings.SRVs[(int)EMaterialTextureSlot::Diffuse],
			static_cast<int32>(Command.RenderState.DepthStencil),
			static_cast<int32>(Command.RenderState.Rasterizer),
			static_cast<int32>(Command.RenderState.Blend));

		++LoggedCommands;
		++CommandIndex;
	}
}

static void LogPreviewTextureProbe(ID3D11Device* Device, ID3D11DeviceContext* Context, ID3D11Texture2D* Texture)
{
	if (!Device || !Context || !Texture)
	{
		UE_LOG_CATEGORY(FbxImporter, Warning, "ObjViewer preview RT probe skipped: missing D3D resource");
		return;
	}

	D3D11_TEXTURE2D_DESC Desc = {};
	Texture->GetDesc(&Desc);
	if (Desc.Width == 0 || Desc.Height == 0 || Desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM)
	{
		UE_LOG_CATEGORY(
			FbxImporter,
			Warning,
			"ObjViewer preview RT probe skipped: Size=%ux%u Format=%u",
			Desc.Width,
			Desc.Height,
			static_cast<uint32>(Desc.Format));
		return;
	}

	D3D11_TEXTURE2D_DESC StagingDesc = Desc;
	StagingDesc.BindFlags = 0;
	StagingDesc.MiscFlags = 0;
	StagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	StagingDesc.Usage = D3D11_USAGE_STAGING;

	ID3D11Texture2D* StagingTexture = nullptr;
	HRESULT Hr = Device->CreateTexture2D(&StagingDesc, nullptr, &StagingTexture);
	if (FAILED(Hr) || !StagingTexture)
	{
		UE_LOG_CATEGORY(FbxImporter, Warning, "ObjViewer preview RT probe staging creation failed: 0x%08X", static_cast<uint32>(Hr));
		return;
	}

	Context->CopyResource(StagingTexture, Texture);

	D3D11_MAPPED_SUBRESOURCE Mapped = {};
	Hr = Context->Map(StagingTexture, 0, D3D11_MAP_READ, 0, &Mapped);
	if (FAILED(Hr))
	{
		UE_LOG_CATEGORY(FbxImporter, Warning, "ObjViewer preview RT probe map failed: 0x%08X", static_cast<uint32>(Hr));
		StagingTexture->Release();
		return;
	}

	const uint32 StepX = (std::max)(1u, Desc.Width / 64u);
	const uint32 StepY = (std::max)(1u, Desc.Height / 64u);
	uint32 ReferencePixel = 0;
	bool bHasReferencePixel = false;
	uint32 Samples = 0;
	uint32 DifferentFromFirst = 0;
	uint8 MinR = 255, MinG = 255, MinB = 255;
	uint8 MaxR = 0, MaxG = 0, MaxB = 0;

	for (uint32 Y = 0; Y < Desc.Height; Y += StepY)
	{
		const uint8* Row = static_cast<const uint8*>(Mapped.pData) + static_cast<size_t>(Y) * Mapped.RowPitch;
		for (uint32 X = 0; X < Desc.Width; X += StepX)
		{
			const uint32 Pixel = *reinterpret_cast<const uint32*>(Row + static_cast<size_t>(X) * 4);
			if (!bHasReferencePixel)
			{
				ReferencePixel = Pixel;
				bHasReferencePixel = true;
			}
			else if (Pixel != ReferencePixel)
			{
				++DifferentFromFirst;
			}

			const uint8 B = static_cast<uint8>((Pixel >> 0) & 0xFF);
			const uint8 G = static_cast<uint8>((Pixel >> 8) & 0xFF);
			const uint8 R = static_cast<uint8>((Pixel >> 16) & 0xFF);
			MinR = (std::min)(MinR, R);
			MinG = (std::min)(MinG, G);
			MinB = (std::min)(MinB, B);
			MaxR = (std::max)(MaxR, R);
			MaxG = (std::max)(MaxG, G);
			MaxB = (std::max)(MaxB, B);
			++Samples;
		}
	}

	const uint32 CenterX = Desc.Width / 2u;
	const uint32 CenterY = Desc.Height / 2u;
	const uint8* CenterRow = static_cast<const uint8*>(Mapped.pData) + static_cast<size_t>(CenterY) * Mapped.RowPitch;
	const uint32 CenterPixel = *reinterpret_cast<const uint32*>(CenterRow + static_cast<size_t>(CenterX) * 4);
	const uint8 CenterB = static_cast<uint8>((CenterPixel >> 0) & 0xFF);
	const uint8 CenterG = static_cast<uint8>((CenterPixel >> 8) & 0xFF);
	const uint8 CenterR = static_cast<uint8>((CenterPixel >> 16) & 0xFF);
	const uint8 CenterA = static_cast<uint8>((CenterPixel >> 24) & 0xFF);

	Context->Unmap(StagingTexture, 0);
	StagingTexture->Release();

	UE_LOG_CATEGORY(
		FbxImporter,
		Info,
		"ObjViewer preview RT probe: Size=%ux%u Samples=%u DifferentFromFirst=%u RGBMin=(%u,%u,%u) RGBMax=(%u,%u,%u) CenterBGRA=(%u,%u,%u,%u)",
		Desc.Width,
		Desc.Height,
		Samples,
		DifferentFromFirst,
		MinR,
		MinG,
		MinB,
		MaxR,
		MaxG,
		MaxB,
		CenterB,
		CenterG,
		CenterR,
		CenterA);
}

static void AddAllSceneProxiesForPreview(FScene& Scene, FCollectOutput& Output)
{
	for (FPrimitiveSceneProxy* Proxy : Scene.GetAllProxies())
	{
		if (!Proxy || !Proxy->IsVisible())
		{
			continue;
		}

		Output.FrustumVisibleProxies.push_back(Proxy);
		Output.RenderableProxies.push_back(Proxy);
		Output.VisibleProxySet.insert(Proxy);
	}
}

void FObjViewerRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
	// 오프스크린 RT에 3D 씬 렌더
	RenderPreviewViewport(Renderer);

	// 스왑체인 백버퍼 → ImGui 합성 → Present
	Renderer.BeginFrame();
	Engine->RenderUI(DeltaTime);
	Renderer.EndFrame();
}

void FObjViewerRenderPipeline::RenderPreviewViewport(FRenderer& Renderer)
{
	FObjViewerViewportClient* VC = Engine->GetViewportClient();
	if (!VC) return;

	UCameraComponent* Camera = VC->GetCamera();
	if (!Camera) return;

	FViewport* VP = VC->GetViewport();
	if (!VP) return;

	ID3D11DeviceContext* Ctx = Renderer.GetFD3DDevice().GetDeviceContext();

	// 지연 리사이즈 적용 + 오프스크린 RT 바인딩
	if (VP->ApplyPendingResize())
	{
		Camera->OnResize(static_cast<int32>(VP->GetWidth()), static_cast<int32>(VP->GetHeight()));
	}
	const float ClearColor[4] = { 0.15f, 0.15f, 0.15f, 1.0f };
	VP->BeginRender(Ctx, ClearColor);

	// Frame 설정
	Frame.ClearViewportResources();

	UWorld* World = Engine->GetWorld();
	FScene& Scene = World->GetScene();
	Scene.ClearFrameData();

	Frame.SetCameraInfo(Camera);

	FViewportRenderOptions Opts;
	Opts.ViewMode = EViewMode::Unlit;
	Opts.ShowFlags.bGrid = false;
	Opts.ShowFlags.bGizmo = false;
	Opts.ShowFlags.bBillboardText = false;
	Opts.ShowFlags.bBoundingVolume = false;
	Opts.ShowFlags.bGammaCorrection = false;
	Frame.SetRenderOptions(Opts);
	Frame.SetViewportInfo(VP);

	// BeginCollect → 데이터 수집 → 커맨드 생성 → Render
	FDrawCommandBuilder& Builder = Renderer.GetBuilder();
	Builder.BeginCollect(Frame, Scene.GetProxyCount());
	FCollectOutput Output;
	Collector.Collect(World, Frame, Output);
	if (Output.RenderableProxies.empty() && Scene.GetProxyCount() > 0)
	{
		AddAllSceneProxiesForPreview(Scene, Output);
	}
	Builder.BuildCommands(Frame, &Scene, Output);
	ForcePreviewMeshRenderState(Builder.GetCommandList());
	const bool bLogPreviewFrame = Engine->ConsumePreviewFrameLog();
	if (bLogPreviewFrame)
	{
		FDrawCommandList& Commands = Builder.GetCommandList();
		UE_LOG_CATEGORY(
			FbxImporter,
			Info,
			"ObjViewer preview render: SceneProxies=%d FrustumVisible=%d Renderable=%d DrawCommands=%d PreDepth=%d Opaque=%d GammaCorrection=%d PreviewNoPreDepthNoCull=1",
			static_cast<int32>(Scene.GetProxyCount()),
			static_cast<int32>(Output.FrustumVisibleProxies.size()),
			static_cast<int32>(Output.RenderableProxies.size()),
			static_cast<int32>(Commands.GetCommandCount()),
			static_cast<int32>(CountCommandsForPass(Commands, ERenderPass::PreDepth)),
			static_cast<int32>(CountCommandsForPass(Commands, ERenderPass::Opaque)),
			static_cast<int32>(CountCommandsForPass(Commands, ERenderPass::GammaCorrection)));
		LogPreviewProxySections(Output);
		LogPreviewDrawCommands(Commands);
	}
	Renderer.Render(Frame, Scene);
	if (bLogPreviewFrame)
	{
		LogPreviewTextureProbe(Renderer.GetFD3DDevice().GetDevice(), Ctx, VP->GetRTTexture());
	}
}
