#include "SceneManager.h"
#include "Scene.h"
#include "TitleScene.h"
#include "PlayScene.h"
#include "Core/GameContext.h"
#include "Gameplay/Stage.h"

void FSceneManager::Initialize(FGameContext* InGameContext)
{
	GameContext = InGameContext;
	ChangeSceneImmediately(ESceneType::Title);
}

void FSceneManager::RequestChangeScene(ESceneType SceneType)
{
	PendingSceneType = SceneType;
	bHasPendingSceneChange = true;
}

void FSceneManager::ChangeSceneImmediately(ESceneType SceneType)
{
	if (CurrentScene)
	{
		CurrentScene->Exit();
	}

	CurrentScene = CreateScene(SceneType);
	CurrentSceneType = SceneType;

	if (CurrentScene)
	{
		CurrentScene->Enter();
	}
}

void FSceneManager::Update()
{
	ApplyPendingSceneChange();

	if (CurrentScene && GameContext)
	{
		CurrentScene->Update(*GameContext);

		if (CurrentScene->HasSceneChangeRequest())
		{
			RequestChangeScene(CurrentScene->GetRequestedScene());
			CurrentScene->ClearSceneChangeRequest();
		}
	}
}

void FSceneManager::Render()
{
	if (CurrentScene && GameContext)
	{
		CurrentScene->Render(*GameContext);
	}
}

ESceneType FSceneManager::GetCurrentSceneType() const
{
	return CurrentSceneType;
}

IScene* FSceneManager::GetCurrentScene()
{
	return CurrentScene.get();
}

const IScene* FSceneManager::GetCurrentScene() const
{
	return CurrentScene.get();
}

void FSceneManager::ApplyPendingSceneChange()
{
	if (bHasPendingSceneChange)
	{
		bHasPendingSceneChange = false;
		ChangeSceneImmediately(PendingSceneType);
	}
}

std::unique_ptr<IScene> FSceneManager::CreateScene(ESceneType SceneType)
{
	switch (SceneType)
	{
	case ESceneType::Title:
		return std::make_unique<FTitleScene>();
	case ESceneType::Play:
	{
		auto Scene = std::make_unique<FPlayScene>();
		if (GameContext)
		{
			Scene->SetRenderer(&GameContext->Renderer);
			Scene->SetTextureManager(&GameContext->Textures);
		}
		return Scene;
	}
	default:
		return nullptr;
	}
}
