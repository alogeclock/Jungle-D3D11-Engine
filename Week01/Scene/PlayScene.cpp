#include "PlayScene.h"
#include "Core/GameContext.h"
#include "Core/Time.h"
#include "Gameplay/Stage.h"

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
}

void FPlayScene::RestartGame()
{
	StartNewGame(CurrentMapPath, CurrentStageIndex);
}
