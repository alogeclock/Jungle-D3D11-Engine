
#include "pch.h"
#include "Scene.h"
#include "SceneType.h"
#include "UI/popup/UIPopupAction.h"
#include "UI/popup/UIPopupBase.h"

FSceneCommand IScene::ConsumeCommand()
{
    FSceneCommand Result = PendingCommand;
    PendingCommand = FSceneCommand();
    return Result;
}

void IScene::SetSceneCommand(const FSceneCommand &Command) { PendingCommand = Command; }

void IScene::ChangeScene(ESceneType NextScene)
{
    FSceneCommand Command;
    Command.Type = ESceneCommandType::ChangeScene;
    Command.NextScene = NextScene;
    SetSceneCommand(Command);
}

void IScene::ChangeScene(ESceneType NextScene, int NextStageIndex, int AccumulatedScore)
{
    FSceneCommand Command;
    Command.Type = ESceneCommandType::ChangeScene;
    Command.NextScene = NextScene;
    Command.NextStageIndex = NextStageIndex;
    Command.AccumulatedScore = AccumulatedScore;
    SetSceneCommand(Command);
}

void IScene::QuitGame()
{
    FSceneCommand Command;
    Command.Type = ESceneCommandType::QuitGame;
    SetSceneCommand(Command);
}

void IScene::DispatchPopupAction(FGameContext &Context, FUIPopupBase &Popup, EUIPopupAction Action)
{
    OnPopupActionDispatched(Action);

    switch (Action)
    {
    case EUIPopupAction::None:
        return;

    case EUIPopupAction::ClosePopup:
        Popup.Close();
        return;

    case EUIPopupAction::GoToTitleScene:
        Popup.Close();
        ChangeScene(ESceneType::Title);
        return;

    case EUIPopupAction::OpenGoToTitlePopup:
        Popup.Close();
        OpenGoToTitlePopup();
        return;

    default:
        break;
    }

    HandleOwnPopupAction(Context, Popup, Action);
}

bool IScene::HandleOwnPopupAction(FGameContext &Context, FUIPopupBase &Popup, EUIPopupAction Action)
{
    return false;
}

void IScene::OnPopupActionDispatched(EUIPopupAction Action) {}