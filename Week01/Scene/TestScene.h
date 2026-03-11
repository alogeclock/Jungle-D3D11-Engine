#pragma once

#include <string>
#include "Scene/Scene.h"
#include "UI/UIManager.h"

struct FGameContext;
class FUIPopupBase;
enum class EUIPopupAction;

class FTestScene : public IScene
{
  public:
    void Update(FGameContext &Context) override;
    void Render(FGameContext &Context) override;

  private:
    void RenderBackground(FGameContext &Context);
    void RenderMenu(FGameContext &Context);
    void HandleMenuCommand(FGameContext &Context);
    void HandlePopupResult(FGameContext &Context);

    void OpenCreditPopup();
    void OpenScoreboardPopup();
    void OpenGameOverPopup();
    void OpenStageClearPopup(bool bAllCleared);
    void OpenSaveScorePopup();
    void OpenGoToTitlePopup() override;

    bool HandleOwnPopupAction(FGameContext &Context, FUIPopupBase &Popup,
                              EUIPopupAction Action) override;
    void OnPopupActionDispatched(EUIPopupAction Action) override;

    void               SetLastActionText(const char *Text);
    static const char *ToActionText(EUIPopupAction Action);

  private:
    FUIManager  UIManager;
    std::string LastActionText = "None";
};