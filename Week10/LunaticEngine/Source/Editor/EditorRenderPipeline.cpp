#include "EditorRenderPipeline.h"
#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Editor/Viewport/Preview/PreviewViewportClient.h"
#include "Render/Pipeline/Renderer.h"
#include "Render/Scene/FScene.h"
#include "Viewport/Viewport.h"
#include "Component/CameraComponent.h"
#include "Engine/Camera/PlayerCameraManager.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/World.h"
#include "Profiling/Stats.h"
#include "Profiling/GPUProfiler.h"
#include "Engine/Render/Types/ForwardLightData.h"
#include "Component/Light/LightComponentBase.h"
#include "Core/ProjectSettings.h"
#include "Viewport/GameViewportClient.h"
#include "Viewport/ViewportClient.h"
#include "Object/Object.h"

FEditorRenderPipeline::FEditorRenderPipeline(UEditorEngine* InEditor, FRenderer& InRenderer)
	: Editor(InEditor)
	, CachedDevice(InRenderer.GetFD3DDevice().GetDevice())
{
}

FEditorRenderPipeline::~FEditorRenderPipeline()
{
}

void FEditorRenderPipeline::OnSceneCleared()
{
	for (auto& [VC, Occlusion] : GPUOcclusionMap)
	{
		Occlusion->InvalidateResults();
	}

	for (FLevelEditorViewportClient* VC : Editor->GetLevelViewportClients())
	{
		VC->ClearLightViewOverride();
	}
}

FGPUOcclusionCulling& FEditorRenderPipeline::GetOcclusionForViewport(FLevelEditorViewportClient* VC)
{
	auto it = GPUOcclusionMap.find(VC);
	if (it != GPUOcclusionMap.end())
		return *it->second;

	auto ptr = std::make_unique<FGPUOcclusionCulling>();
	ptr->Initialize(CachedDevice);
	auto& ref = *ptr;
	GPUOcclusionMap.emplace(VC, std::move(ptr));
	return ref;
}

void FEditorRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
#if STATS
	FStatManager::Get().TakeSnapshot();
	FGPUProfiler::Get().TakeSnapshot();
	FGPUProfiler::Get().BeginFrame();
#endif

	// 이전 프레임 시각화 데이터 readback + 디버그 라인 제출
	Renderer.SubmitCullingDebugLines(Editor->GetWorld());

	// Shadow depth는 라이트 시점 → 뷰포트 무관. 프레임당 1회만 렌더링.
	++Renderer.GetResources().ShadowResources.FrameGeneration;

	for (FLevelEditorViewportClient* ViewportClient : Editor->GetLevelViewportClients())
	{
		if (!Editor->ShouldRenderViewportClient(ViewportClient)) continue;
		SCOPE_STAT_CAT("RenderEditorViewport", "2_Render");
		RenderViewport(ViewportClient, Renderer);
	}

	for (FPreviewViewportClient* ViewportClient : Editor->GetPreviewViewportClients())
	{
		if (!ViewportClient) continue;
		SCOPE_STAT_CAT("RenderPreviewViewport", "2_Render");
		RenderViewport(ViewportClient, Renderer);
	}

	// 스왑체인 백버퍼 복귀 → ImGui 합성 → Present
	Renderer.BeginFrame();
	{
		SCOPE_STAT_CAT("EditorUI", "5_UI");
		Editor->RenderUI(DeltaTime);
	}

#if STATS
	FGPUProfiler::Get().EndFrame();
#endif

	{
		SCOPE_STAT_CAT("Present", "2_Render");
		Renderer.EndFrame();
	}
}

void FEditorRenderPipeline::RenderViewport(FViewportClient* VC, FRenderer& Renderer)
{
	if (!VC) return;

	UWorld* World = nullptr;
	UCameraComponent* Camera = nullptr;
	FViewport* VP = nullptr;
	FGPUOcclusionCulling* Occlusion = nullptr;

	if (FLevelEditorViewportClient* LevelVC = dynamic_cast<FLevelEditorViewportClient*>(VC))
	{
		World = Editor->GetWorld();
		Camera = LevelVC->GetCamera();
		VP = LevelVC->GetViewport();
		Occlusion = &GetOcclusionForViewport(LevelVC);

		// PIE 고려한 카메라 결정 로직
		if (Editor && Editor->IsPIEPossessedMode())
		{
			if (UGameViewportClient* GameViewportClient = Editor->GetGameViewportClient())
			{
				if (UCameraComponent* GameCamera = GameViewportClient->GetDrivingCamera())
				{
					if (IsAliveObject(GameCamera) && GameCamera->GetWorld() == World)
					{
						Camera = GameCamera;
					}
				}
			}

			if (!Camera || !IsAliveObject(Camera) || Camera->GetWorld() != World)
			{
				Camera = (World ? World->GetActiveCamera() : nullptr);
			}
		}
	}
	else if (FPreviewViewportClient* PreviewVC = dynamic_cast<FPreviewViewportClient*>(VC))
	{
		World = PreviewVC->GetWorld();
		Camera = PreviewVC->GetCamera();
		VP = PreviewVC->GetViewport();
	}

	if (!World || !Camera || !IsAliveObject(Camera) || !VP) return;

	ID3D11DeviceContext* Ctx = Renderer.GetFD3DDevice().GetDeviceContext();
	if (!Ctx) return;

	// GPU Occlusion — 이전 프레임 결과 읽기 (LevelEditor 전용)
	if (Occlusion)
	{
		Occlusion->ReadbackResults(Ctx);
	}

	PrepareViewport(VP, Camera, Ctx);
	
	if (FEditorViewportClient* EVC = dynamic_cast<FEditorViewportClient*>(VC))
	{
		EVC->SetupFrameContext(Frame, Camera, VP, World);
	}
	else
	{
		Frame.ClearViewportResources();
		if (Camera)
		{
			Frame.SetCameraInfo(Camera);
		}
		Frame.SetViewportInfo(VP);
	}

	// 파이프라인 전용 설정 주입 (Occlusion)
	Frame.OcclusionCulling = Occlusion;

	FCollectOutput Output;
	CollectCommands(World, Renderer, Output);

	FScene& Scene = World->GetScene();

	{
		SCOPE_STAT_CAT("Renderer.Render", "4_ExecutePass");
		Renderer.Render(Frame, Scene);
	}

	// GPU Occlusion — Render 후 DepthBuffer가 유효할 때 디스패치 (LevelEditor 전용)
	if (Occlusion)
	{
		SCOPE_STAT_CAT("GPUOcclusion", "4_ExecutePass");
		Occlusion->DispatchOcclusionTest(
			Ctx,
			VP->GetDepthCopySRV(),
			Output.FrustumVisibleProxies,
			Frame.View, Frame.Proj,
			VP->GetWidth(), VP->GetHeight());
	}
}

// ============================================================
// PrepareViewport — 지연 리사이즈 적용 + RT 클리어
// ============================================================
void FEditorRenderPipeline::PrepareViewport(FViewport* VP, UCameraComponent* Camera, ID3D11DeviceContext* Ctx)
{
	if (VP->ApplyPendingResize())
	{
		Camera->OnResize(static_cast<int32>(VP->GetWidth()), static_cast<int32>(VP->GetHeight()));
	}
	const float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	VP->BeginRender(Ctx, ClearColor);
}

// ============================================================
// CollectCommands — Scene 데이터 주입 + DrawCommand 생성
// ============================================================
//
// 3단계로 구성:
//   1. Proxy   — frustum cull → DrawCommand 즉시 생성 (메시/폰트/데칼)
//   2. Debug   — Scene에 디버그 데이터 주입 (Grid, DebugDraw, Octree, ShadowFrustum)
//   3. UI      — Scene에 오버레이 텍스트 주입
//
// 마지막에 BuildDynamicCommands가 Scene 주입 데이터를 DrawCommand로 변환.

void FEditorRenderPipeline::CollectCommands(UWorld* World, FRenderer& Renderer, FCollectOutput& Output)
{
	SCOPE_STAT_CAT("Collector", "3_Collect");

	FScene& Scene = World->GetScene();
	Scene.ClearFrameData();

	FDrawCommandBuilder& Builder = Renderer.GetBuilder();
	Builder.BeginCollect(Frame, Scene.GetProxyCount());

	const FShowFlags& Flags = Frame.RenderOptions.ShowFlags;

	// ── 1. 데이터 수집: frustum cull + visibility/occlusion 필터 ──
	{
		SCOPE_STAT_CAT("Collect", "3_Collect");
		Collector.Collect(World, Frame, Output);
	}

	// ── 2. Debug: Scene에 디버그 데이터 주입 ──
	{
		SCOPE_STAT_CAT("CollectDebug", "3_Collect");
		const bool bAllowDebugVisuals = World && World->GetWorldType() != EWorldType::PIE;
		if (bAllowDebugVisuals)
		{
			Collector.CollectGrid(Frame.RenderOptions.GridSpacing, Frame.RenderOptions.GridHalfLineCount, Scene);
			Scene.SetLightVisualizationSettings(
				Flags.bLightVisualization,
				Frame.RenderOptions.DirectionalLightVisualizationScale,
				Frame.RenderOptions.PointLightVisualizationScale,
				Frame.RenderOptions.SpotLightVisualizationScale);

			if (Flags.bShowShadowFrustum)
				Scene.SubmitShadowFrustumDebug(World, Frame);

			if (Flags.bSceneBVH)
				Collector.CollectSceneBVHDebug(World, Scene);

			if (Flags.bOctree)
				Collector.CollectOctreeDebug(World->GetOctree(), Scene);

			if (Flags.bWorldBound)
				Collector.CollectWorldBoundsDebug(World, Scene);

			Collector.CollectDebugDraw(Frame, Scene);
		}
		else
		{
			Scene.SetLightVisualizationSettings(false, 0.0f, 0.0f, 0.0f);
		}
	}

	// ── 3. 커맨드 일괄 생성 (프록시 + 동적) ──
	{
		SCOPE_STAT_CAT("BuildCommands", "3_Collect");
		Builder.BuildCommands(Frame, &Scene, Output);
	}
}

