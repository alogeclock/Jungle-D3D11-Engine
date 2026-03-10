#include "pch.h"
#include "PlayScene.h"
#include "Core/AudioSystem.h"
#include "Core/GameContext.h"
#include "Core/Time.h"
#include "Data/ScoreRepository.h"
#include "Data/StageLoader.h"
#include "Gameplay/Stage.h"
#include "UI/BeatHUDWidget.h"
#include "UI/DebugWidget.h"
#include "UI/GameplayHUDWidget.h"
#include <filesystem>

FPlayScene::~FPlayScene() = default;

void FPlayScene::SetRenderer(FRenderer *InRenderer) { Renderer = InRenderer; }

void FPlayScene::SetTextureManager(FTextureManager *InTextures) { Textures = InTextures; }

void FPlayScene::Enter() { StartNewGame(0); }

void FPlayScene::Exit()
{
    FAudioSystem::Get().StopAll();
    Stage.reset();
    UIManager.ClearAll();
}

void FPlayScene::Update(FGameContext &Context)
{
    // 지연된 스테이지 변경 처리 (UI 순회 중 안전하게)
    if (PendingStageIndex >= 0)
    {
        int Index = PendingStageIndex;
        PendingStageIndex = -1;
        StartNewGame(Index);
        return;
    }

    if (Stage && !bIsPaused)
    {
        Stage->Update(Context.Time.GetDeltaTime(), Context);

        // 스테이지 클리어 시
        if (Stage->IsCleared())
        {
            int NextIndex = CurrentStageIndex + 1;
            int TotalStages = FStageLoader::Get().GetStageCount();
            int CarryScore = Stage->GetScoreSystem().GetScore();

            if (NextIndex < TotalStages)
            {
                StartNewGame(NextIndex);
                Stage->GetScoreSystem().SetScore(CarryScore);
            }
            else
            {
                // 모든 스테이지 클리어 → 저장 후 타이틀로
                SaveScore();
                RequestSceneChange(ESceneType::Title);
            }
            return;
        }

        // 게임오버 시 → 저장 후 타이틀로
        if (Stage->IsGameOver())
        {
            SaveScore();
            RequestSceneChange(ESceneType::Title);
            return;
        }
    }

    UIManager.Update(Context);
}

void FPlayScene::Render(FGameContext &Context)
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
    Stage->Load(CurrentStageIndex, Renderer, Textures);

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
    Debug->SetTotalStages(FStageLoader::Get().GetStageCount());
    Debug->SetStageChangeCallback([this](int Index) { PendingStageIndex = Index; });
    UIManager.AddWidget("Debug", std::move(Debug));
}

void FPlayScene::RestartGame() { StartNewGame(CurrentStageIndex); }

void FPlayScene::SaveScore()
{
    if (!Stage) return;

    const std::string SaveDir = "Save";
    const std::string SavePath = SaveDir + "/scoreboard.json";

    std::filesystem::create_directories(SaveDir);

    FScoreRepository Repo;
    auto Records = Repo.Load(SavePath);

    FScoreRecord Record;
    Record.Name = "Player";
    Record.Stage = Stage->GetCurrentStageIndex() + 1;
    Record.Score = Stage->GetScoreSystem().GetScore();

    Repo.AddRecord(Records, Record);
    Repo.SortDescending(Records);
    Repo.Save(SavePath, Records);
}
