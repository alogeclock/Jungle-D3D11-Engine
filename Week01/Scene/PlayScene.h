#pragma once

#include <memory>
#include "Scene/Scene.h"
#include "UI/UIManager.h"

class FStage;

class FPlayScene : public IScene
{
  public:
    explicit FPlayScene(int InStageIndex = 0);
    ~FPlayScene() override = default;

    void Update(FGameContext &Context) override;
    void Render(FGameContext &Context) override;

  private:
    void LoadStage(FGameContext &Context);
    void HandleStageResult(FGameContext &Context);
    void HandlePopupResult(FGameContext &Context);

  private:
    std::unique_ptr<FStage> Stage;
    FUIManager              UIManager;

    int  CurrentStageIndex = 0;
    bool bIsPaused = false;
    bool bStageLoaded = false;
    bool bOpenSaveScorePopupNextFrame = false;
};
