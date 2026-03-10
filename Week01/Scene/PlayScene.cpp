#include "pch.h"
#include "PlayScene.h"
#include "Core/GameContext.h"
#include "Core/Time.h"
#include "Data/StageLoader.h"
#include "Gameplay/Stage.h"
#include "UI/BeatHUDWidget.h"
#include "UI/DebugWidget.h"
#include "UI/GameplayHUDWidget.h"

FPlayScene::~FPlayScene() = default;

void FPlayScene::SetRenderer(FRenderer* InRenderer) { Renderer = InRenderer; }

void FPlayScene::SetTextureManager(FTextureManager* InTextures) { Textures = InTextures; }

void FPlayScene::SetFontManager(FFontManager* InFontManager)
{
	FontManager = InFontManager;
}

void FPlayScene::Enter() { StartNewGame(0); }

void FPlayScene::Exit()
{
	Stage.reset();
	UIManager.ClearAll();
}

void FPlayScene::Update(FGameContext& Context)
{
	if (Stage && !bIsPaused)
	{
		Stage->Update(Context.Time.GetDeltaTime(), Context);

		// 스테이지 클리어 시
		if (Stage->IsCleared())
		{
			int NextIndex = CurrentStageIndex + 1;
			int TotalStages = FStageLoader::Get().GetStageCount();

			if (NextIndex < TotalStages)
			{
				StartNewGame(NextIndex);
			}
			else
			{
				// 모든 스테이지 클리어 → 타이틀로
				RequestSceneChange(ESceneType::Title);
			}
			return;
		}

		// 게임오버 시 → 타이틀로
		if (Stage->IsGameOver())
		{
			RequestSceneChange(ESceneType::Title);
			return;
		}
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

void FPlayScene::StartNewGame(int StageIndex)
{
	CurrentStageIndex = StageIndex;

	Stage = std::make_unique<FStage>();
	Stage->Load(CurrentStageIndex, Renderer, Textures, FontManager);

	bIsPaused = false;

	// HUD 위젯 등록
	UIManager.ClearAll();
	auto HUD = std::make_unique<FGameplayHUDWidget>();
	HUD->BindStage(Stage.get());
	HUD->BindPauseFlag(&bIsPaused);
	UIManager.AddWidget("GameplayHUD", std::move(HUD));

	auto BeatHUD = std::make_unique<FBeatHUDWidget>();
	BeatHUD->BindBeatSystem(&Stage->GetBeatSystem());
	UIManager.AddWidget("BeatHUD", std::move(BeatHUD));

	auto Debug = std::make_unique<FDebugWidget>();
	Debug->BindStage(Stage.get());
	UIManager.AddWidget("Debug", std::move(Debug));
}

void FPlayScene::RestartGame() { StartNewGame(CurrentStageIndex); }
