#pragma once

#include "Scene/Scene.h"
#include "UI/UIManager.h"

struct FGameContext;
class FUIPopupBase;
enum class EUIPopupAction;

class FTitleScene : public IScene
{
  public:
    FTitleScene() = default;
    ~FTitleScene() override = default;

    void Update(FGameContext &Context) override;
    void Render(FGameContext &Context) override;

  private:
    bool bInitialized = false;
    int  SelectedPlayerSkin = 0;
    void RenderBackground(FGameContext &Context);
    void RenderTitleMenu(FGameContext &Context);
    void HandleMenuCommand(FGameContext &Context);
    void HandlePopupResult(FGameContext &Context);

    void OpenCreditPopup();
    void OpenScoreboardPopup();

    bool HandleOwnPopupAction(FGameContext &Context, FUIPopupBase &Popup,
                              EUIPopupAction Action) override;

  private:
    FUIManager UIManager;
};
