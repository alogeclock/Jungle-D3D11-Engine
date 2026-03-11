#pragma once

#include <string>
#include "UIPopupAction.h"
#include "UIPopupBase.h"

class FSaveScorePopup : public FUIPopupBase
{
  public:
    FSaveScorePopup();

    void SetScore(int InScore) { Score = InScore; }
    void SetStage(int InStage) { Stage = InStage; }
    void SetNickname(const std::string &InNickname);
    const std::string &GetNickname() const { return Nickname; }

    EUIPopupAction ConsumeAction();
    void Render(FGameContext &Context) override;
    void Update(FGameContext &Context) override {}

  private:
    static constexpr EUIPopupContentAlign ContentAlign = EUIPopupContentAlign::Center;
    static constexpr EUIPopupContentTextSize ContentTextSize = EUIPopupContentTextSize::Medium;
    static constexpr int MaxNicknameLength = 6;

  private:
    void SyncBufferFromNickname();
    void SyncNicknameFromBuffer();

  private:
    int Score = 0;
    int Stage = 0;
    std::string Nickname = "";
    char NicknameBuffer[MaxNicknameLength + 1];
    EUIPopupAction PendingAction = EUIPopupAction::None;
};