#include "pch.h"
#include "PlayScene.h"
#include "Core/AudioSystem.h"
#include <memory>
#include <string>
#include "Core/GameContext.h"
#include "Core/Time.h"
#include "Data/ScoreRepository.h"
#include "Data/StageLoader.h"
#include "Gameplay/Stage.h"
#include "Scene/SceneCommand.h"
#include "Scene/SceneType.h"
#include "UI/popup/GameOverPopup.h"
#include "UI/popup/PopupManager.h"
#include "UI/popup/SaveScorePopup.h"
#include "UI/popup/StageClearPopup.h"
#include "UI/popup/UIPopupAction.h"
#include "UI/widget/BeatHUDWidget.h"
#include "UI/widget/DebugWidget.h"
#include "UI/widget/GameplayHUDWidget.h"

FPlayScene::FPlayScene(int InStageIndex) : CurrentStageIndex(InStageIndex) {}

void FPlayScene::Update(FGameContext &Context)
{
    if (!bStageLoaded)
    {
        LoadStage(Context);
        bStageLoaded = true;
    }

    // 지연된 스테이지 변경 처리 (UI 순회 중 안전하게)
    if (PendingStageIndex >= 0)
    {
        FSceneCommand Command;
        Command.Type = ESceneCommandType::ChangeScene;
        Command.NextScene = ESceneType::Play;
        Command.NextStageIndex = PendingStageIndex;
        PendingStageIndex = -1;
        SetSceneCommand(Command);
        return;
    }

    FPopupManager &PopupManager = UIManager.GetPopupManager();

    bIsPaused = PopupManager.HasOpenPopup();

    if (Stage && !bIsPaused)
    {
        Stage->Update(Context.Time.GetDeltaTime(), Context);
        HandleStageResult(Context);
    }

    UIManager.Update(Context);
    HandlePopupResult(Context);
    PopupManager.RemoveClosedPopup();

    if (bOpenSaveScorePopupNextFrame && !PopupManager.HasOpenPopup())
    {
        std::unique_ptr<FSaveScorePopup> Popup = std::make_unique<FSaveScorePopup>();
        Popup->SetScore(Stage ? Stage->GetScore() : 0);
        PopupManager.Open(std::move(Popup));
        bOpenSaveScorePopupNextFrame = false;
    }
}

void FPlayScene::Render(FGameContext &Context)
{
    // TODO: 인자로 컨텍스트 넘겨서 내부 렌더러 호출하도록 변경
    if (Stage)
        Stage->Render();
    UIManager.Render(Context);
}

void FPlayScene::LoadStage(FGameContext &Context)
{
    Stage = std::make_unique<FStage>();
    Stage->Load(CurrentStageIndex, &Context.Renderer, &Context.Textures);

    bIsPaused = false;

    // HUD 위젯 등록
    UIManager.ClearAll();

    std::unique_ptr<FGameplayHUDWidget> HUD = std::make_unique<FGameplayHUDWidget>();
    HUD->BindStage(Stage.get());
    HUD->BindPauseFlag(&bIsPaused);
    UIManager.AddWidget("GameplayHUD", std::move(HUD));

    std::unique_ptr<FBeatHUDWidget> BeatHUD = std::make_unique<FBeatHUDWidget>();
    BeatHUD->BindBeatSystem(&Stage->GetBeatSystem());
    UIManager.AddWidget("BeatHUD", std::move(BeatHUD));

    std::unique_ptr<FDebugWidget> Debug = std::make_unique<FDebugWidget>();
    Debug->BindStage(Stage.get());
    Debug->SetTotalStages(FStageLoader::Get().GetStageCount());
    Debug->SetStageChangeCallback([this](int Index) { PendingStageIndex = Index; });
    UIManager.AddWidget("Debug", std::move(Debug));
}

void FPlayScene::HandleStageResult(FGameContext &Context)
{
    if (!Stage)
        return;

    FPopupManager &PopupManager = UIManager.GetPopupManager();

    if (PopupManager.HasOpenPopup())
        return;

    if (Stage->IsGameOver())
    {
        PopupManager.Open(std::make_unique<FGameOverPopup>());
        return;
    }

    if (Stage->IsCleared())
    {
        const int NextIndex = CurrentStageIndex + 1;
        const int TotalStages = FStageLoader::Get().GetStageCount();
        const bool bAllCleared = (NextIndex >= TotalStages);

        std::unique_ptr<FStageClearPopup> Popup = std::make_unique<FStageClearPopup>();
        Popup->SetData(bAllCleared, CurrentStageIndex + 1);
        PopupManager.Open(std::move(Popup));
        return;
    }
}
#include <iostream>

void FPlayScene::HandlePopupResult(FGameContext &Context)
{
std::cout << "What\n";
    

    FPopupManager &PopupManager = UIManager.GetPopupManager();

    if (FStageClearPopup *Popup = PopupManager.GetPopup<FStageClearPopup>())
    {
        switch (Popup->ConsumeAction())
        {
        case EUIPopupAction::None:
            break;

        case EUIPopupAction::OpenSaveScorePopup:
        {
            Popup->Close();
            bOpenSaveScorePopupNextFrame = true;
            break;
        }

        case EUIPopupAction::GoToNextStage:
        {
            Popup->Close();

            FSceneCommand Command;
            Command.Type = ESceneCommandType::ChangeScene;
            Command.NextScene = ESceneType::Play;
            Command.NextStageIndex = CurrentStageIndex + 1;
            SetSceneCommand(Command);
            break;
        }

        case EUIPopupAction::GoToTitleScene:
        {
            Popup->Close();

            FSceneCommand Command;
            Command.Type = ESceneCommandType::ChangeScene;
            Command.NextScene = ESceneType::Title;
            SetSceneCommand(Command);
            break;
        }

        case EUIPopupAction::ClosePopup:
        {
            Popup->Close();
            break;
        }

        default:
            break;
        }

        return;
    }

    if (FGameOverPopup *Popup = PopupManager.GetPopup<FGameOverPopup>())
    {
        switch (Popup->ConsumeAction())
        {
        case EUIPopupAction::None:
            break;

        case EUIPopupAction::RetryCurrentStage:
        {
            Popup->Close();

            FSceneCommand Command;
            Command.Type = ESceneCommandType::ChangeScene;
            Command.NextScene = ESceneType::Play;
            Command.NextStageIndex = CurrentStageIndex;
            SetSceneCommand(Command);
            break;
        }

        case EUIPopupAction::GoToTitleScene:
        {
            Popup->Close();

            FSceneCommand Command;
            Command.Type = ESceneCommandType::ChangeScene;
            Command.NextScene = ESceneType::Title;
            SetSceneCommand(Command);
            break;
        }

        case EUIPopupAction::ClosePopup:
        {
            Popup->Close();
            break;
        }

        default:
            break;
        }

        return;
    }

    if (FSaveScorePopup *Popup = PopupManager.GetPopup<FSaveScorePopup>())
    {
        switch (Popup->ConsumeAction())
        {
        case EUIPopupAction::None:
            break;

        case EUIPopupAction::ConfirmSaveScore:
        {
            Popup->Close();

            const std::string Nickname = Popup->GetNickname();
            const int ClearedStage = CurrentStageIndex + 1;
            const int Score = Stage ? Stage->GetScore() : 0;

            // TODO: 스코어 저장 및 닉네임 예외처리
            // 예:
            // FScoreRepository::Get().AddScore(Nickname, Score, ClearedStage);

            FSceneCommand Command;
            Command.Type = ESceneCommandType::ChangeScene;
            Command.NextScene = ESceneType::Title;
            SetSceneCommand(Command);
            break;
        }

        case EUIPopupAction::CancelSaveScore:
        {
            Popup->Close();

            FSceneCommand Command;
            Command.Type = ESceneCommandType::ChangeScene;
            Command.NextScene = ESceneType::Title;
            SetSceneCommand(Command);
            break;
        }

        case EUIPopupAction::ClosePopup:
        {
            Popup->Close();
            break;
        }

        default:
            break;
        }

        return;
    }
}