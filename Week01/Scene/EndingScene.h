#pragma once

#include <string>
#include <vector>
#include "Scene/Scene.h"
#include "UI/UIManager.h"

struct FGameContext;
class FUIPopupBase;
enum class EUIPopupAction;

class FEndingScene : public IScene
{
  public:
    FEndingScene() = default;
    ~FEndingScene() override = default;

    void Update(FGameContext &Context) override;
    void Render(FGameContext &Context) override;

private:
    void HandlePopupResult(FGameContext &Context);
    void OpenEndingPopup();
    void OpenSaveScorePopup();
    void OpenGoToTitlePopup() override;

    bool HandleOwnPopupAction(FGameContext &Context, FUIPopupBase &Popup,
                              EUIPopupAction Action) override;

  private:
    FUIManager               UIManager;
    float                    ElapsedTime = 0.0f;
    bool                     bOpenedEndingPopup = false;
    std::vector<std::string> Messages = {
        "You found the final treasure.",
        "The rhythm of the jungle is now yours.",
        "Thank you for playing."
    };

  private:
    static constexpr const char *BackgroundTextureKey = "ending_background";
    static constexpr float       PopupOpenDelay = 1.0f;
};
