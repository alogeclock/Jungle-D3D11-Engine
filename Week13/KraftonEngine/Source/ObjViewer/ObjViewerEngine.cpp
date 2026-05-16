#include "ObjViewer/ObjViewerEngine.h"

#include "ObjViewer/ObjViewerRenderPipeline.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Mesh/MeshManager.h"
#include "Component/StaticMeshComponent.h"
#include "GameFramework/AActor.h"
#include "Viewport/Viewport.h"

IMPLEMENT_CLASS(UObjViewerEngine, UEngine)

void UObjViewerEngine::Init(FWindowsWindow* InWindow)
{
	UEngine::Init(InWindow);

	FMeshManager::ScanMeshAssets();
	FEditorObjImportService::ScanObjSourceFiles();
	FEditorFbxImportService::ScanFbxSourceFiles();

	// ImGui 패널 초기화
	Panel.Create(InWindow, Renderer, this);

	// World
	if (WorldList.empty())
	{
		CreateWorldContext(EWorldType::Game, FName("ObjViewer"));
	}
	SetActiveWorld(WorldList[0].ContextHandle);
	GetWorld()->InitWorld();

	// 뷰포트 클라이언트 + 오프스크린 RT
	ViewportClient.Initialize(InWindow);
	ViewportClient.CreateCamera();
	ViewportClient.ResetCamera();

	FViewport* VP = new FViewport();
	VP->Initialize(Renderer.GetFD3DDevice().GetDevice(),
		static_cast<uint32>(InWindow->GetWidth()),
		static_cast<uint32>(InWindow->GetHeight()));
	VP->SetClient(&ViewportClient);
	ViewportClient.SetViewport(VP);

	// ObjViewer 전용 렌더 파이프라인
	SetRenderPipeline(std::make_unique<FObjViewerRenderPipeline>(this, Renderer));
}

void UObjViewerEngine::Shutdown()
{
	ViewportClient.Release();
	Panel.Release();

	for (FWorldContext& Ctx : WorldList)
	{
		Ctx.World->EndPlay();
		UObjectManager::Get().DestroyObject(Ctx.World);
	}
	WorldList.clear();
	ActiveWorldHandle = FName::None;

	UEngine::Shutdown();
}

void UObjViewerEngine::Tick(float DeltaTime)
{
	ViewportClient.Tick(DeltaTime);
	Panel.Update();
	UEngine::Tick(DeltaTime);
}

void UObjViewerEngine::RenderUI(float DeltaTime)
{
	Panel.Render(DeltaTime);
}

void UObjViewerEngine::LoadPreviewMesh(const FString& MeshPath)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 기존 액터 모두 제거
	TArray<AActor*> Actors = World->GetActors();
	for (AActor* Actor : Actors)
	{
		World->DestroyActor(Actor);
	}

	// 메시 로드
	ID3D11Device* Device = Renderer.GetFD3DDevice().GetDevice();
	UStaticMesh* Mesh = FMeshManager::LoadStaticMesh(MeshPath, Device);
	if (!Mesh) return;

	// 프리뷰 액터 생성
	AActor* PreviewActor = World->SpawnActor<AActor>();
	if (!PreviewActor) return;

	UStaticMeshComponent* MeshComp = PreviewActor->AddComponent<UStaticMeshComponent>();
	MeshComp->SetStaticMesh(Mesh);
	PreviewActor->SetRootComponent(MeshComp);

	// 카메라 리셋
	ViewportClient.ResetCamera();
}

void UObjViewerEngine::ImportObjWithOptions(const FString& ObjPath, const FImportOptions& Options)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 기존 액터 모두 제거
	TArray<AActor*> Actors = World->GetActors();
	for (AActor* Actor : Actors)
	{
		World->DestroyActor(Actor);
	}

	// 옵션 기반 OBJ import 후 생성된 .uasset을 프리뷰한다.
	ID3D11Device* Device = Renderer.GetFD3DDevice().GetDevice();
	UStaticMesh* Mesh = nullptr;
	FEditorObjImportService::ImportStaticMeshFromObj(ObjPath, Options, Device, Mesh);
	if (!Mesh) return;

	// 프리뷰 액터 생성
	AActor* PreviewActor = World->SpawnActor<AActor>();
	if (!PreviewActor) return;

	UStaticMeshComponent* MeshComp = PreviewActor->AddComponent<UStaticMeshComponent>();
	MeshComp->SetStaticMesh(Mesh);
	PreviewActor->SetRootComponent(MeshComp);

	ViewportClient.ResetCamera();
}

void UObjViewerEngine::ImportFbxWithOptions(const FString& FbxPath, const FImportOptions& Options)
{
	UWorld* World = GetWorld();
	if (!World) return;

	TArray<AActor*> Actors = World->GetActors();
	for (AActor* Actor : Actors)
	{
		World->DestroyActor(Actor);
	}

	ID3D11Device* Device = Renderer.GetFD3DDevice().GetDevice();
	UStaticMesh* Mesh = nullptr;
	FEditorFbxImportService::ImportStaticMeshFromFbx(FbxPath, Options, Device, Mesh);
	if (!Mesh) return;

	AActor* PreviewActor = World->SpawnActor<AActor>();
	if (!PreviewActor) return;

	UStaticMeshComponent* MeshComp = PreviewActor->AddComponent<UStaticMeshComponent>();
	MeshComp->SetStaticMesh(Mesh);
	PreviewActor->SetRootComponent(MeshComp);

	ViewportClient.ResetCamera();
}
