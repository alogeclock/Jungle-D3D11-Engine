#pragma once

#include <string>
#include "Scene/Scene.h"
#include "UI/UIManager.h"

struct FGameContext;

class FTestScene : public IScene
{
  public:
    FTestScene() = default;
    ~FTestScene() override = default;

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
    void OpenGoToTitlePopup();

    void SetLastActionText(const char *Text);

  private:
    FUIManager  UIManager;
    std::string LastActionText = "None";
};