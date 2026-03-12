#pragma once

#include <string>
#include "Scene/Scene.h"
#include "UI/UIManager.h"

struct FGameContext;
class FUIPopupBase;

class FEndingScene : public IScene
{
  public:
    explicit FEndingScene(int InTotalScore = 0, const std::string &InPlayerSkinKey = "")
        : TotalScore(InTotalScore), PlayerSkinKey(InPlayerSkinKey)
    {
    }

    ~FEndingScene() override = default;

    void Update(FGameContext &Context) override;
    void Render(FGameContext &Context) override;

  private:
    void RenderBackground(FGameContext &Context);
    void OpenEndingPopup();
    void OpenSaveScorePopup();
    void OpenGoToTitlePopup();

    bool HandleOwnPopupAction(FGameContext &Context, FUIPopupBase &Popup);
    void HandlePopupResult(FGameContext &Context);
    void ChangeToTitleScene(FGameContext &Context);

  private:
    FUIManager   UIManager;
    float        ElapsedTime = 0.0f;
    bool         bOpenedEndingPopup = false;
    int          TotalScore = 0;
    std::string  PlayerSkinKey;
};
