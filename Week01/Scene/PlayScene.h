#pragma once

#include <string>
#include "Scene.h"
#include "UIManager.h"

class FStage;
class FPopupManager;

class FPlayScene : public IScene
{
  public:
    void Enter() override;
    void Exit() override;

    void Update(FGameContext &Context) override;
    void Render(FGameContext &Context) override;

    void StartNewGame(const std::string &MapPath);
    void RestartGame();

  private:
    FStage    *Stage = nullptr;
    FUIManager UIManager;

    bool bIsPaused = false;
    bool bIsGameOverPopupOpened = false;

    std::string CurrentMapPath;
};
