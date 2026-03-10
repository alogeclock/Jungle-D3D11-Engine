#include "PlayScene.h"
#include "../Core/GameContext.h"
#include "../Core/Time.h"
#include "../Gameplay/Stage.h"
#include "../Gameplay/PopupManager.h"

FPlayScene::~FPlayScene() = default;

void FPlayScene::SetRenderer(URenderer* InRenderer)
{
	Renderer = InRenderer;
}

void FPlayScene::SetTextureManager(FTextureManager* InTextures)
{
	Textures = InTextures;
}

void FPlayScene::Enter()
{
	PopupManager = std::make_unique<FPopupManager>();
	StartNewGame("Resources/Maps/default.map");
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
		Stage->Update(Context.Time.GetDeltaTime());
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
		Stage->Render();
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
	Stage->Load(CurrentMapPath, Renderer, Textures);
}

void FPlayScene::RestartGame()
{
	StartNewGame(CurrentMapPath);
}
