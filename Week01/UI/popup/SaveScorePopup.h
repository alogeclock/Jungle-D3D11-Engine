#pragma once

#include <string>
#include "UIPopupBase.h"

struct FGameContext;

class FSaveScorePopup : public FUIPopupBase
{
  public:
    FSaveScorePopup();

    void SetNickname(const std::string &InNickname);
    void SetScore(int InScore) { Score = InScore; }
    void SetStage(int InStage) { Stage = InStage; }

    const std::string &GetNickname() const { return Nickname; }

    EUIPopupAction ConsumeAction();
    void            Render(FGameContext &Context);
    void Update(FGameContext &Context) {}

  private:
    void SyncBufferFromNickname();
    void SyncNicknameFromBuffer();
    bool IsValidNickname() const;

  private:
    static constexpr EUIPopupContentTextSize ContentTextSize = EUIPopupContentTextSize::Big;
    static constexpr int MaxNicknameLength = 6;

    std::string    Nickname;
    char           NicknameBuffer[MaxNicknameLength + 1]{};
    int            Score = 0;
    int            Stage = 0;
    EUIPopupAction PendingAction = EUIPopupAction::None;
    bool           bShowValidationMessage = false;
};
