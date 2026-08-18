#include "ObjViewer/ObjViewerEngine.h"

#include "ObjViewer/ObjViewerRenderPipeline.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Core/Log.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Mesh/ObjManager.h"
#include "Mesh/FbxImporter.h"
#include "Mesh/StaticMeshAsset.h"
#include "Component/StaticMeshComponent.h"
#include "GameFramework/AActor.h"
#include "Viewport/Viewport.h"
#include "Engine/Input/InputRouter.h"
#include "Engine/Input/InputSystem.h"
#include "Platform/Paths.h"

#include <filesystem>

IMPLEMENT_CLASS(UObjViewerEngine, UEngine)

static FString ToGenericPathString(const std::filesystem::path& Path)
{
	return FPaths::ToUtf8(Path.generic_wstring());
}

static bool IsRegularFbxFile(const std::filesystem::path& Path)
{
	return std::filesystem::is_regular_file(Path)
		&& Path.has_extension()
		&& Path.extension().wstring() == L".fbx";
}

static FString ResolveFbxPreviewPath(const FString& InputPath)
{
	if (InputPath.empty())
	{
		return InputPath;
	}

	std::filesystem::path Path(FPaths::ToWide(InputPath));
	if (IsRegularFbxFile(Path))
	{
		return ToGenericPathString(Path);
	}

	if (!Path.has_extension())
	{
		std::filesystem::path WithExtension = Path;
		WithExtension += L".fbx";
		if (IsRegularFbxFile(WithExtension))
		{
			return ToGenericPathString(WithExtension);
		}
	}

	if (std::filesystem::is_directory(Path))
	{
		for (const std::filesystem::directory_entry& Entry : std::filesystem::directory_iterator(Path))
		{
			if (IsRegularFbxFile(Entry.path()))
			{
				return ToGenericPathString(Entry.path());
			}
		}
	}

	return InputPath;
}

void UObjViewerEngine::Init(FWindowsWindow* InWindow)
{
	UEngine::Init(InWindow);

	FObjManager::ScanMeshAssets();
	FObjManager::ScanObjSourceFiles();

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
	FInputRouter::Get().SetKeyboardFocusedViewport(&ViewportClient);
	FInputRouter::Get().SetHoveredViewport(&ViewportClient);

	// ObjViewer 전용 렌더 파이프라인
	SetRenderPipeline(std::make_unique<FObjViewerRenderPipeline>(this, Renderer));
}

void UObjViewerEngine::Shutdown()
{
	FInputRouter::Get().ClearViewport(&ViewportClient);
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
	FInputRouter::Get().RouteSnapshot(FInputSystem::MakeSnapshot(), DeltaTime);
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

	// 메시 로드
	ID3D11Device* Device = Renderer.GetFD3DDevice().GetDevice();
	UStaticMesh* Mesh = FObjManager::LoadObjStaticMesh(MeshPath, Device);
	if (!Mesh) return;

	FStaticMesh* MeshAsset = Mesh->GetStaticMeshAsset();
	if (!MeshAsset || MeshAsset->Vertices.empty() || MeshAsset->Indices.empty())
	{
		UE_LOG_CATEGORY(ObjImporter, Error, "OBJ preview mesh is empty: %s", MeshPath.c_str());
		return;
	}

	if (!MeshAsset->bBoundsValid)
	{
		MeshAsset->CacheBounds();
	}

	// 기존 액터 모두 제거
	TArray<AActor*> Actors = World->GetActors().ToArray();
	for (AActor* Actor : Actors)
	{
		World->DestroyActor(Actor);
	}

	// 프리뷰 액터 생성
	AActor* PreviewActor = World->SpawnActor<AActor>();
	if (!PreviewActor) return;

	UStaticMeshComponent* MeshComp = PreviewActor->AddComponent<UStaticMeshComponent>();
	MeshComp->SetStaticMesh(Mesh);
	PreviewActor->SetRootComponent(MeshComp);
	World->GetPartition().FlushPrimitive();

	ViewportClient.ResetCameraToBounds(MeshAsset->BoundsCenter, MeshAsset->BoundsExtent);
	RequestPreviewFrameLog();
	UE_LOG_CATEGORY(
		ObjImporter,
		Info,
		"OBJ preview mesh: Path=%s Vertices=%d Indices=%d Sections=%d BoundsCenter=(%.3f, %.3f, %.3f) BoundsExtent=(%.3f, %.3f, %.3f)",
		MeshPath.c_str(),
		static_cast<int32>(MeshAsset->Vertices.size()),
		static_cast<int32>(MeshAsset->Indices.size()),
		static_cast<int32>(MeshAsset->Sections.size()),
		MeshAsset->BoundsCenter.X,
		MeshAsset->BoundsCenter.Y,
		MeshAsset->BoundsCenter.Z,
		MeshAsset->BoundsExtent.X,
		MeshAsset->BoundsExtent.Y,
		MeshAsset->BoundsExtent.Z);
}

void UObjViewerEngine::LoadPreviewFbxStaticMesh(const FString& FbxPath)
{
	UWorld* World = GetWorld();
	if (!World) return;

	const FString ResolvedFbxPath = ResolveFbxPreviewPath(FbxPath);
	FStaticMesh* MeshAsset = new FStaticMesh();
	TArray<FStaticMaterial> Materials;

	if (!FFbxImporter::ImportStaticMesh(ResolvedFbxPath, *MeshAsset, Materials))	{
		UE_LOG_CATEGORY(FbxImporter, Error, "FBX static mesh import failed: %s", ResolvedFbxPath.c_str());
		delete MeshAsset;
		return;
	}

	UStaticMesh* Mesh = UObjectManager::Get().CreateObject<UStaticMesh>();
	if (!Mesh)
	{
		delete MeshAsset;
		return;
	}
	Mesh->SetStaticMaterials(std::move(Materials));
	Mesh->SetStaticMeshAsset(MeshAsset);
	Mesh->InitResources(Renderer.GetFD3DDevice().GetDevice());

	TArray<AActor*> Actors = World->GetActors().ToArray();
	for (AActor* ExistingActor : Actors)
	{
		World->DestroyActor(ExistingActor);
	}

	AActor* Actor = World->SpawnActor<AActor>();
	if (!Actor) return;
	UStaticMeshComponent* Comp = Actor->AddComponent<UStaticMeshComponent>();
	Comp->SetStaticMesh(Mesh);
	Actor->SetRootComponent(Comp);
	World->GetPartition().FlushPrimitive();

	ViewportClient.ResetCameraToBounds(MeshAsset->BoundsCenter, MeshAsset->BoundsExtent);
	RequestPreviewFrameLog();
	UE_LOG_CATEGORY(
		FbxImporter,
		Info,
		"FBX preview mesh: Path=%s Vertices=%d Indices=%d Sections=%d BoundsCenter=(%.3f, %.3f, %.3f) BoundsExtent=(%.3f, %.3f, %.3f)",
		ResolvedFbxPath.c_str(),
		static_cast<int32>(MeshAsset->Vertices.size()),
		static_cast<int32>(MeshAsset->Indices.size()),
		static_cast<int32>(MeshAsset->Sections.size()),
		MeshAsset->BoundsCenter.X,
		MeshAsset->BoundsCenter.Y,
		MeshAsset->BoundsCenter.Z,
		MeshAsset->BoundsExtent.X,
		MeshAsset->BoundsExtent.Y,
		MeshAsset->BoundsExtent.Z);
}

void UObjViewerEngine::ImportObjWithOptions(const FString& ObjPath, const FImportOptions& Options)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 옵션 기반 메시 로드 (캐시 무효화 + .bin 저장)
	ID3D11Device* Device = Renderer.GetFD3DDevice().GetDevice();
	UStaticMesh* Mesh = FObjManager::LoadObjStaticMesh(ObjPath, Options, Device);
	if (!Mesh) return;

	FStaticMesh* MeshAsset = Mesh->GetStaticMeshAsset();
	if (!MeshAsset || MeshAsset->Vertices.empty() || MeshAsset->Indices.empty())
	{
		UE_LOG_CATEGORY(ObjImporter, Error, "Imported OBJ preview mesh is empty: %s", ObjPath.c_str());
		return;
	}

	if (!MeshAsset->bBoundsValid)
	{
		MeshAsset->CacheBounds();
	}

	// 기존 액터 모두 제거
	TArray<AActor*> Actors = World->GetActors().ToArray();
	for (AActor* Actor : Actors)
	{
		World->DestroyActor(Actor);
	}

	// 프리뷰 액터 생성
	AActor* PreviewActor = World->SpawnActor<AActor>();
	if (!PreviewActor) return;

	UStaticMeshComponent* MeshComp = PreviewActor->AddComponent<UStaticMeshComponent>();
	MeshComp->SetStaticMesh(Mesh);
	PreviewActor->SetRootComponent(MeshComp);
	World->GetPartition().FlushPrimitive();

	// 리프레시 + 카메라 리셋
	FObjManager::ScanObjSourceFiles();
	ViewportClient.ResetCameraToBounds(MeshAsset->BoundsCenter, MeshAsset->BoundsExtent);
	RequestPreviewFrameLog();
	UE_LOG_CATEGORY(
		ObjImporter,
		Info,
		"Imported OBJ preview mesh: Path=%s Vertices=%d Indices=%d Sections=%d BoundsCenter=(%.3f, %.3f, %.3f) BoundsExtent=(%.3f, %.3f, %.3f)",
		ObjPath.c_str(),
		static_cast<int32>(MeshAsset->Vertices.size()),
		static_cast<int32>(MeshAsset->Indices.size()),
		static_cast<int32>(MeshAsset->Sections.size()),
		MeshAsset->BoundsCenter.X,
		MeshAsset->BoundsCenter.Y,
		MeshAsset->BoundsCenter.Z,
		MeshAsset->BoundsExtent.X,
		MeshAsset->BoundsExtent.Y,
		MeshAsset->BoundsExtent.Z);
}

void UObjViewerEngine::LogFbxSceneSummary(const FString& FbxPath)
{
	UE_LOG_CATEGORY(FbxImporter, Warning, "LogFbxSceneSummary is deprecated in the new FbxImporter.");
}
