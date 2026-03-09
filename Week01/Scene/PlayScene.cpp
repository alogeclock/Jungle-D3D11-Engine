#include "PlayScene.h"
#include "../Core/GameContext.h"
#include "../Gameplay/Stage.h"
#include "../Gameplay/PopupManager.h"

FPlayScene::~FPlayScene() = default;

void FPlayScene::Enter()
{
	PopupManager = std::make_unique<FPopupManager>();
	StartNewGame("maps/default.map");
}

void FPlayScene::Exit()
{
	Stage.reset();
	PopupManager.reset();
}

void FPlayScene::Update(FGameContext& Context)
{
	if (Stage && !bIsPaused)
	{
		Stage->Update(Context);
	}

	if (PopupManager)
	{
		PopupManager->Update(Context);
	}
}

void FPlayScene::Render(FGameContext& Context)
{
	if (Stage)
	{
		Stage->Render(Context);
	}

	if (PopupManager)
	{
		PopupManager->Render(Context);
	}
}

void FPlayScene::StartNewGame(const std::string& MapPath)
{
	CurrentMapPath = MapPath;

	Stage = std::make_unique<FStage>();
	Stage->Load(CurrentMapPath);
}

void FPlayScene::RestartGame()
{
	StartNewGame(CurrentMapPath);
}
