#include "PreviewScene.h"

#include "Core/TickFunction.h"
#include "GameFramework/Light/AmbientLightActor.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "Object/ObjectFactory.h"

#include <algorithm>

namespace
{
	void AddDefaultPreviewLights(UWorld* World);
}

// 에디터 월드와 분리된 전용 UWorld 생성 및 기본 조명 설정
void FPreviewScene::Initialize()
{
	if (PreviewWorld)
	{
		return;
	}

	PreviewWorld = UObjectManager::Get().CreateObject<UWorld>();
	if (!PreviewWorld)
	{
		return;
	}

	PreviewWorld->SetWorldType(EWorldType::Editor);
	PreviewWorld->InitWorld();
	AddDefaultPreviewLights(PreviewWorld);
}

void FPreviewScene::Release()
{
	Clear();

	if (PreviewWorld)
	{
		UObjectManager::Get().DestroyObject(PreviewWorld);
		PreviewWorld = nullptr;
	}
}

void FPreviewScene::Tick(float DeltaTime)
{
	if (!PreviewWorld)
	{
		return;
	}

	PreviewWorld->Tick(DeltaTime, LEVELTICK_ViewportsOnly);
}

void FPreviewScene::Clear()
{
	if (!PreviewWorld)
	{
		PreviewActors.clear();
		return;
	}

	while (!PreviewActors.empty())
	{
		AActor* Actor = PreviewActors.back();
		PreviewActors.pop_back();
		if (Actor)
		{
			PreviewWorld->DestroyActor(Actor);
		}
	}
}

AActor* FPreviewScene::SpawnPreviewActor()
{
	Initialize();
	if (!PreviewWorld)
	{
		return nullptr;
	}

	AActor* Actor = PreviewWorld->SpawnActor<AActor>();
	if (Actor)
	{
		PreviewActors.push_back(Actor);
	}
	return Actor;
}

void FPreviewScene::DestroyPreviewActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	auto It = std::find(PreviewActors.begin(), PreviewActors.end(), Actor);
	if (It != PreviewActors.end())
	{
		PreviewActors.erase(It);
	}

	if (PreviewWorld)
	{
		PreviewWorld->DestroyActor(Actor);
	}
}

namespace
{
	void AddDefaultPreviewLights(UWorld* World)
	{
		if (!World)
		{
			return;
		}

		if (AAmbientLightActor* AmbientLight = World->SpawnActor<AAmbientLightActor>())
		{
			AmbientLight->InitDefaultComponents();
		}

		if (ADirectionalLightActor* DirectionalLight = World->SpawnActor<ADirectionalLightActor>())
		{
			DirectionalLight->InitDefaultComponents();
			DirectionalLight->SetActorRotation(FVector(45.0f, -35.0f, 35.0f));
		}
	}
}
