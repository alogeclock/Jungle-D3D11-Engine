#pragma once

#include "Scene/Scene.h"
#include "UI/UIManager.h"

struct FGameContext;

class FTitleScene : public IScene
{
  public:
    FTitleScene() = default;
    ~FTitleScene() override = default;

    void Update(FGameContext &Context) override;
    void Render(FGameContext &Context) override;

  private:
    void HandleMenuCommand();
    void OpenCreditPopup();
    void OpenScoreboardPopup();

  private:
    FUIManager UIManager;
};
