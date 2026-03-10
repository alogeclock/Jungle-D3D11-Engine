#include "PlayScene.h"
#include "Core/GameContext.h"
#include "Core/Time.h"
#include "Gameplay/Stage.h"
#include "UI/GameplayHUDWidget.h"

FPlayScene::~FPlayScene() = default;

void FPlayScene::SetRenderer(FRenderer* InRenderer)
{
	Renderer = InRenderer;
}

void FPlayScene::SetTextureManager(FTextureManager* InTextures)
{
	Textures = InTextures;
}

void FPlayScene::Enter()
{
	StartNewGame("Resources/Maps/stages.json", 0);
}

void FPlayScene::Exit()
{
	Stage.reset();
	UIManager.ClearAll();
}

void FPlayScene::Update(FGameContext& Context)
{
	if (Stage && !bIsPaused)
	{
		Stage->Update(Context.Time.GetDeltaTime());
	}

	UIManager.Update(Context);
}

void FPlayScene::Render(FGameContext& Context)
{
	if (Stage)
	{
		Stage->Render();
	}

	UIManager.Render(Context);
}

void FPlayScene::StartNewGame(const std::string& MapPath, int StageIndex)
{
	CurrentMapPath = MapPath;
	CurrentStageIndex = StageIndex;

	Stage = std::make_unique<FStage>();
	Stage->Load(CurrentMapPath, CurrentStageIndex, Renderer, Textures);

	// HUD 위젯 등록
	UIManager.ClearAll();
	auto HUD = std::make_unique<FGameplayHUDWidget>();
	HUD->BindStage(Stage.get());
	UIManager.AddWidget("GameplayHUD", std::move(HUD));
}

void FPlayScene::RestartGame()
{
	StartNewGame(CurrentMapPath, CurrentStageIndex);
}
