#include "pch.h"
#include "EndingScene.h"

#include <memory>
#include <vector>

#include "Core/GameContext.h"
#include "Core/Time.h"
#include "Data/ScoreRepository.h"
#include "Data/StageLoader.h"
#include "Scene/SceneCommand.h"
#include "Scene/SceneType.h"
#include "UI/popup/EndingPopup.h"
#include "UI/popup/GoToTitlePopup.h"
#include "UI/popup/PopupManager.h"
#include "UI/popup/SaveScorePopup.h"
#include "UI/popup/UIPopupAction.h"

#define WIN_WIDTH 1024
#define WIN_HEIGHT 1024

void FEndingScene::Update(FGameContext &Context)
{
    ElapsedTime += Context.Time.GetDeltaTime();
    UIManager.Update(Context);
}

void FEndingScene::Render(FGameContext &Context)
{
    RenderBackground(Context);

    if (!bOpenedEndingPopup && ElapsedTime >= 1.0f)
    {
        OpenEndingPopup();
        bOpenedEndingPopup = true;
    }

    UIManager.Render(Context);

    HandlePopupResult(Context);
    UIManager.GetPopupManager().RemoveClosedPopup();
}

void FEndingScene::RenderBackground(FGameContext &Context)
{
    Context.Renderer.DrawTexture(Context.Textures.Get("ending"), (WIN_WIDTH / 2.f),
                                 (WIN_HEIGHT / 2.f), WIN_WIDTH, WIN_HEIGHT);
}

void FEndingScene::OpenEndingPopup()
{
    std::vector<std::string> EndingMessages;
    if (!FStageLoader::Get().LoadEndingMessages(EndingMessages))
    {
        EndingMessages = {""};
    }

    std::unique_ptr<FEndingPopup> Popup = std::make_unique<FEndingPopup>();
    Popup->SetData("Ending", EndingMessages);
    Popup->Open();
    UIManager.GetPopupManager().Open(std::move(Popup));
}

void FEndingScene::OpenSaveScorePopup()
{
    std::unique_ptr<FSaveScorePopup> Popup = std::make_unique<FSaveScorePopup>();
    Popup->SetScore(TotalScore);

    const int TotalStages = FStageLoader::Get().GetStageCount();
    Popup->SetStage(TotalStages > 0 ? TotalStages : 0);

    Popup->Open();
    UIManager.GetPopupManager().Open(std::move(Popup));
}

void FEndingScene::OpenGoToTitlePopup()
{
    std::unique_ptr<FGoToTitlePopup> Popup = std::make_unique<FGoToTitlePopup>();
    Popup->Open();
    UIManager.GetPopupManager().Open(std::move(Popup));
}

void FEndingScene::ChangeToTitleScene(FGameContext &Context)
{
    FSceneCommand Command;
    Command.Type = ESceneCommandType::ChangeScene;
    Command.NextScene = ESceneType::Title;
    SetSceneCommand(Command);
}

bool FEndingScene::HandleOwnPopupAction(FGameContext &Context, FUIPopupBase &Popup,
                                      EUIPopupAction Action)
{
    switch (Action)
    {
    case EUIPopupAction::OpenSaveScorePopup:
        Popup.Close();
        OpenSaveScorePopup();
        return true;

    case EUIPopupAction::ConfirmSaveScore:
    {
        Popup.Close();

        FSaveScorePopup *SavePopup = dynamic_cast<FSaveScorePopup *>(&Popup);
        if (!SavePopup)
            return true;

        const std::string Nickname = SavePopup->GetNickname();
        const int ClearedStage = FStageLoader::Get().GetStageCount();
        const int Score = TotalScore;

        ScoreRepository::AppendRecord({Nickname, ClearedStage, Score});
        OpenGoToTitlePopup();
        return true;
    }

    case EUIPopupAction::GoToTitleScene:
        Popup.Close();
        ChangeToTitleScene(Context);
        return true;

    default:
        return false;
    }
}

void FEndingScene::HandlePopupResult(FGameContext &Context)
{
    FPopupManager &PopupManager = UIManager.GetPopupManager();

    if (FEndingPopup *Popup = PopupManager.GetPopup<FEndingPopup>())
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
