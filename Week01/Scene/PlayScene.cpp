#include "pch.h"
#include "PlayScene.h"
#include <memory>
#include <string>

#include "Core/AudioSystem.h"
#include "Core/GameContext.h"
#include "Core/Time.h"
#include "Data/ScoreRepository.h"
#include "Data/StageData.h"   
#include "Data/StageLoader.h"
#include "Gameplay/Stage.h"
#include "Scene/SceneCommand.h"
#include "Scene/SceneType.h"
#include "UI/popup/GameOverPopup.h"
#include "UI/popup/GoToTitlePopup.h"
#include "UI/popup/PopupManager.h"
#include "UI/popup/SaveScorePopup.h"
#include "UI/popup/StageClearPopup.h"
#include "UI/popup/StageIntroPopup.h"
#include "UI/popup/UIPopupAction.h"
#include "UI/widget/BeatHUDWidget.h"
#include "UI/widget/DebugWidget.h"
#include "UI/widget/GameplayHUDWidget.h"
#include "UI/widget/MinimapWidget.h"

FPlayScene::FPlayScene(int InStageIndex, int InAccumulatedScore)
    : CurrentStageIndex(InStageIndex), AccumulatedScore(InAccumulatedScore)
{
}

void FPlayScene::Update(FGameContext &Context)
{
    if (!bStageLoaded)
    {
        LoadStage(Context);
        bStageLoaded = true;
    }

    if (PendingStageIndex >= 0)
    {
        FSceneCommand Command;
        Command.Type = ESceneCommandType::ChangeScene;
        Command.NextScene = ESceneType::Play;
        Command.NextStageIndex = PendingStageIndex;
        Command.AccumulatedScore = Stage ? Stage->GetScore() : 0;
        PendingStageIndex = -1;
        SetSceneCommand(Command);
        return;
    }

    FPopupManager &PopupManager = UIManager.GetPopupManager();

    bIsPaused = bWaitingForStageStart || PopupManager.HasOpenPopup();

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
        Popup->Open();
        PopupManager.Open(std::move(Popup));
        bOpenSaveScorePopupNextFrame = false;
    }
}

void FPlayScene::Render(FGameContext &Context)
{
    if (Stage)
        Stage->Render();

    UIManager.Render(Context);
}

void FPlayScene::LoadStage(FGameContext &Context)
{
    FAudioSystem::Get().StopAll();

    Stage = std::make_unique<FStage>();
    Stage->Load(CurrentStageIndex, &Context.Renderer, &Context.Textures);
    Stage->GetScoreSystem().SetScore(AccumulatedScore);

    bIsPaused = false;
    bWaitingForStageStart = true;

    UIManager.ClearAll();

    auto HUD = std::make_unique<FGameplayHUDWidget>();
    HUD->SetTextures(Context);
    HUD->BindStage(Stage.get());
    HUD->BindPauseFlag(&bIsPaused);
    UIManager.AddWidget("GameplayHUD", std::move(HUD));

    auto BeatHUD = std::make_unique<FBeatHUDWidget>();
    BeatHUD->SetTextures(Context);
    BeatHUD->BindBeatSystem(&Stage->GetBeatSystem());
    Stage->GetScoreSystem().SetJudgeCallback(
        [HUDPtr = BeatHUD.get()](EBeatJudge Judge)
        {
            HUDPtr->OnBeatJudged(Judge);
        });
    UIManager.AddWidget("BeatHUD", std::move(BeatHUD));

    auto Minimap = std::make_unique<FMinimapWidget>();
    Minimap->BindStage(Stage.get());
    UIManager.AddWidget("Minimap", std::move(Minimap));

    auto Debug = std::make_unique<FDebugWidget>();
    Debug->BindStage(Stage.get());
    Debug->SetTotalStages(FStageLoader::Get().GetStageCount());
    Debug->SetStageChangeCallback([this](int Index) { PendingStageIndex = Index; });
    UIManager.AddWidget("Debug", std::move(Debug));

    FStageData StageData;
    if (FStageLoader::Get().LoadStageById(CurrentStageIndex, StageData))
    {
        OpenStageIntroPopup(StageData.GetIntroMessage());
    }
    else
    {
        OpenStageIntroPopup("");
    }
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
        FAudioSystem::Get().StopAll();

        std::unique_ptr<FGameOverPopup> Popup = std::make_unique<FGameOverPopup>();
        Popup->Open();
        PopupManager.Open(std::move(Popup));
        return;
    }

    if (Stage->IsCleared())
    {
        FAudioSystem::Get().StopAll();

        const int  NextIndex = CurrentStageIndex + 1;
        const int  TotalStages = FStageLoader::Get().GetStageCount();
        const bool bAllCleared = (NextIndex >= TotalStages);

        std::unique_ptr<FStageClearPopup> Popup = std::make_unique<FStageClearPopup>();
        Popup->SetData(bAllCleared, CurrentStageIndex + 1);
        Popup->Open();
        PopupManager.Open(std::move(Popup));
        return;
    }
}

void FPlayScene::OpenStageIntroPopup(const std::string &Message)
{
    std::unique_ptr<FStageIntroPopup> Popup = std::make_unique<FStageIntroPopup>();
    Popup->SetData(CurrentStageIndex + 1, Message);
    Popup->Open();
    UIManager.GetPopupManager().Open(std::move(Popup));
}

void FPlayScene::OpenGoToTitlePopup()
{
    std::unique_ptr<FGoToTitlePopup> Popup = std::make_unique<FGoToTitlePopup>();
    Popup->Open();
    UIManager.GetPopupManager().Open(std::move(Popup));
}

void FPlayScene::HandlePopupResult(FGameContext &Context)
{
    FPopupManager &PopupManager = UIManager.GetPopupManager();

    if (FStageIntroPopup *Popup = PopupManager.GetPopup<FStageIntroPopup>())
    {
        DispatchPopupAction(Context, *Popup, Popup->ConsumeAction());
        return;
    }

    if (FStageClearPopup *Popup = PopupManager.GetPopup<FStageClearPopup>())
    {
        DispatchPopupAction(Context, *Popup, Popup->ConsumeAction());
        return;
    }

    if (FGameOverPopup *Popup = PopupManager.GetPopup<FGameOverPopup>())
    {
        DispatchPopupAction(Context, *Popup, Popup->ConsumeAction());
        return;
    }

    if (FSaveScorePopup *Popup = PopupManager.GetPopup<FSaveScorePopup>())
    {
        DispatchPopupAction(Context, *Popup, Popup->ConsumeAction());
        return;
    }

    if (FGoToTitlePopup *Popup = PopupManager.GetPopup<FGoToTitlePopup>())
    {
        DispatchPopupAction(Context, *Popup, Popup->ConsumeAction());
        return;
    }
}

bool FPlayScene::HandleOwnPopupAction(FGameContext &Context, FUIPopupBase &Popup,
                                      EUIPopupAction Action)
{
    int AccumulatedScore;

    switch (Action)
    {
    case EUIPopupAction::StartStage:
        Popup.Close();
        bWaitingForStageStart = false;
        if (Stage)
            Stage->StartBGM();
        return true;

    case EUIPopupAction::OpenSaveScorePopup:
        Popup.Close();
        bOpenSaveScorePopupNextFrame = true;
        return true;

    case EUIPopupAction::GoToNextStage:
        Popup.Close();
        AccumulatedScore = Stage ? Stage->GetScore() : 0;
        ChangeScene(ESceneType::Play, CurrentStageIndex + 1, AccumulatedScore);
        return true;

    case EUIPopupAction::ConfirmSaveScore:
    {
        Popup.Close();

        FSaveScorePopup *SavePopup = dynamic_cast<FSaveScorePopup *>(&Popup);
        if (!SavePopup)
            return true;

        const std::string Nickname = SavePopup->GetNickname();
        const int         ClearedStage = CurrentStageIndex + 1;
        const int         Score = Stage ? Stage->GetScore() : 0;

        ScoreRepository::AppendRecord({Nickname, ClearedStage, Score});
        OpenGoToTitlePopup();
        return true;
    }

    default:
        return false;
    }
}
