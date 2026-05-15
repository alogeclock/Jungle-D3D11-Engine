#include "AnimSequenceEditorWidget.h"
#include "Object/Object.h"
#include "GameFramework/WorldContext.h"
#include "Runtime/Engine.h"
#include "Slate/SlateApplication.h"
#include "UI/Toolbar/ViewportToolbar.h"
#include "Viewport/Viewport.h"
#include "MeshEditorWidget.h"
#include "GameFramework/StaticMeshActor.h"
#include <imgui.h>

bool FAnimSequenceEditorWidget::CanEdit(UObject* Object) const
{
	return Object; // && Object->IsA<UAnimSequence>();
}

void FAnimSequenceEditorWidget::Open(UObject* Object)
{
	FAssetEditorWidget::Open(Object);

	// FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, PreviewWorldHandle);
	// WorldContext.World->SetWorldType(EWorldType::EditorPreview);
	// WorldContext.World->InitWorld();

	/*
	* TODO: 이 사이에서 에디터 설정에 필요한 요소들을 생성한다.
	* StaticMeshEditorWidget.cpp 참고
	*/

	// AStaticMeshActor* FloorActor = WorldContext.World->SpawnActor<AStaticMeshActor>();
	// FloorActor->InitDefaultComponents("Data/BasicShape/Cube.OBJ");
	// FloorActor->SetActorLocation(FVector(0.0f, 0.0f, -0.05f));
	// FloorActor->SetActorScale(FVector(10.0f, 10.0f, 0.02f));

	ImVec2 ViewportSize = ImGui::GetContentRegionAvail();

	// ViewportClient.Initialize(GEngine->GetRenderer().GetFD3DDevice().GetDevice(), ViewportSize.x, ViewportSize.y);
	// ViewportClient.SetPreviewWorld(WorldContext.World);
	// ViewportClient.SetPreviewActor(Actor);
	// ViewportClient.SetPreviewMeshComponent(Actor->GetComponentByClass<UStaticMeshComponent>());
	// ViewportClient.ResetCameraToPreviewBounds();

	// WorldContext.World->SetEditorPOVProvider(&ViewportClient);

	// FSlateApplication::Get().RegisterViewport(&MeshViewportWindow, &ViewportClient);
}

void FAnimSequenceEditorWidget::Tick(float DeltaTime)
{
	// TODO: 여기서 캐릭터 애니메이션 재생
}

void FAnimSequenceEditorWidget::Render(float DeltaTime)
{
	
}

void FAnimSequenceEditorWidget::Close()
{

}
