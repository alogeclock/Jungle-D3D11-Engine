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
    // 네 기존 엔딩 배경 렌더 코드 유지
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

bool FEndingScene::HandleOwnPopupAction(FGameContext &Context, FUIPopupBase &Popup)
{
    if (FEndingPopup *EndingPopup = dynamic_cast<FEndingPopup *>(&Popup))
    {
        const EUIPopupAction Action = EndingPopup->ConsumeAction();

        switch (Action)
        {
        case EUIPopupAction::OpenSaveScorePopup:
            Popup.Close();
            OpenSaveScorePopup();
            return true;

        default:
            return false;
        }
    }

    if (FSaveScorePopup *SavePopup = dynamic_cast<FSaveScorePopup *>(&Popup))
    {
        const EUIPopupAction Action = SavePopup->ConsumeAction();

        switch (Action)
        {
        case EUIPopupAction::ConfirmSaveScore:
        {
            Popup.Close();

            const std::string Nickname = SavePopup->GetNickname();
            const int ClearedStage = 3;
            const int Score = TotalScore;

            ScoreRepository::AppendRecord({Nickname, ClearedStage, Score});
            OpenGoToTitlePopup();
            return true;
        }

        case EUIPopupAction::ClosePopup:
            Popup.Close();
            return true;

        default:
            return false;
        }
    }

    if (FGoToTitlePopup *GoToTitlePopup = dynamic_cast<FGoToTitlePopup *>(&Popup))
    {
        const EUIPopupAction Action = GoToTitlePopup->ConsumeAction();

        switch (Action)
        {
        case EUIPopupAction::GoToTitleScene:
            Popup.Close();
            ChangeToTitleScene(Context);
            return true;

        case EUIPopupAction::ClosePopup:
            Popup.Close();
            return true;

        default:
            return false;
        }
    }

    return false;
}

void FEndingScene::HandlePopupResult(FGameContext &Context)
{
    if (FUIPopupBase *Popup = UIManager.GetPopupManager().GetPopup<FUIPopupBase>())
    {
        HandleOwnPopupAction(Context, *Popup);
    }
}
