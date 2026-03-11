#pragma once

#include <memory>
#include "Scene/Scene.h"
#include "UI/UIManager.h"

struct FGameContext;
class FStage;
class FUIPopupBase;
enum class EUIPopupAction;

class FPlayScene : public IScene
{
  public:
    explicit FPlayScene(int InStageIndex, int InAccumulatedScore = 0);

    void Update(FGameContext &Context) override;
    void Render(FGameContext &Context) override;

  private:
    void LoadStage(FGameContext &Context);
    void HandleStageResult(FGameContext &Context);
    void HandlePopupResult(FGameContext &Context);
    void OpenGoToTitlePopup() override;

    bool HandleOwnPopupAction(FGameContext &Context, FUIPopupBase &Popup,
                              EUIPopupAction Action) override;

  private:
    FUIManager              UIManager;
    std::unique_ptr<FStage> Stage;

    int  CurrentStageIndex = 0;
    int  PendingStageIndex = -1;
    int  AccumulatedScore = 0;
    bool bStageLoaded = false;
    bool bIsPaused = false;
    bool bOpenSaveScorePopupNextFrame = false;
};