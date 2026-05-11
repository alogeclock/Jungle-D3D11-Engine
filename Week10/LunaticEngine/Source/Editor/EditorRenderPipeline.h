#pragma once

#include "Render/Pipeline/IRenderPipeline.h"
#include "Render/Pipeline/RenderCollector.h"
#include "Render/Types/FrameContext.h"
#include "Render/Culling/GPUOcclusionCulling.h"

#include <memory>

class UEditorEngine;
class FViewport;
class FViewportClient;
class UCameraComponent;
class FLevelEditorViewportClient;
class FPreviewViewportClient;
class UWorld;

class FEditorRenderPipeline : public IRenderPipeline
{
public:
	FEditorRenderPipeline(UEditorEngine* InEditor, FRenderer& InRenderer);
	~FEditorRenderPipeline() override;

	void Execute(float DeltaTime, FRenderer& Renderer) override;
	void OnSceneCleared() override;

private:
	void RenderEditorViewport(FLevelEditorViewportClient* VC, FRenderer& Renderer);
	void RenderPreviewViewport(FPreviewViewportClient* VC, FRenderer& Renderer);

	void PrepareViewport(FViewport* VP, UCameraComponent* Camera, ID3D11DeviceContext* Ctx);
	void BuildFrame(FViewportClient* VC, UCameraComponent* Camera, FViewport* VP, UWorld* World);
	void BuildEditorFrame(FLevelEditorViewportClient* VC, UCameraComponent* Camera, FViewport* VP, UWorld* World);
	void BuildPreviewFrame(FPreviewViewportClient* VC, UCameraComponent* Camera, FViewport* VP, UWorld* World);
	void CollectCommands(UWorld* World, FRenderer& Renderer, FCollectOutput& Output);

	FGPUOcclusionCulling& GetOcclusionForViewport(FLevelEditorViewportClient* VC);

private:
	UEditorEngine* Editor = nullptr;
	ID3D11Device* CachedDevice = nullptr;
	FRenderCollector Collector;
	FFrameContext Frame;
	TMap<FLevelEditorViewportClient*, std::unique_ptr<FGPUOcclusionCulling>> GPUOcclusionMap;
};
