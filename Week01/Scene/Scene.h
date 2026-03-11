#pragma once

#include "SceneCommand.h"

struct FGameContext;
class FUIPopupBase;
enum class EUIPopupAction;
enum class ESceneType;

class IScene
{
  public:
    virtual ~IScene() = default;

    virtual void Update(FGameContext &Context) = 0;
    virtual void Render(FGameContext &Context) = 0;

    FSceneCommand ConsumeCommand();

  protected:
    void SetSceneCommand(const FSceneCommand &Command);

    void ChangeScene(ESceneType NextScene);
    void ChangeScene(ESceneType NextScene, int NextStageIndex);
    void QuitGame();

    void DispatchPopupAction(FGameContext &Context, FUIPopupBase &Popup, EUIPopupAction Action);

    virtual bool HandleOwnPopupAction(FGameContext &Context, FUIPopupBase &Popup,
                                      EUIPopupAction Action);

    virtual void OnPopupActionDispatched(EUIPopupAction Action);

    virtual void OpenGoToTitlePopup() {}

  private:
    FSceneCommand PendingCommand;
};
