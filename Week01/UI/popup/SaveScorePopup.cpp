#include "pch.h"
#include "SaveScorePopup.h"
#include <cstring>
#include <cstdio>

FSaveScorePopup::FSaveScorePopup()
{
    SyncBufferFromNickname();
}

void FSaveScorePopup::SetNickname(const std::string &InNickname)
{
    Nickname = InNickname;
    SyncBufferFromNickname();
}

EUIPopupAction FSaveScorePopup::ConsumeAction()
{
    EUIPopupAction Result = PendingAction;
    PendingAction = EUIPopupAction::None;
    return Result;
}

void FSaveScorePopup::Render(FGameContext &Context)
{
    FPopupFrameLayout Layout;
    if (!BeginPopupWindow("SaveScore", "Save Score",
                          ImVec2(DefaultPopupWidth, DefaultPopupHeight), Layout))
    {
        return;
    }

    char ScoreText[64]{};
    std::snprintf(ScoreText, sizeof(ScoreText), "Score : %d", Score);

    ImGui::SetWindowFontScale(GetContentFontScale(ContentTextSize));

    const ImVec2 ScoreSize = ImGui::CalcTextSize(ScoreText);
    const float ScoreX = GetAlignedX(Layout, ScoreSize.x, EUIPopupContentAlign::Center);
    const float ScoreY = Layout.ContentTop + 24.0f;

    ImGui::SetCursorPos(ImVec2(ScoreX, ScoreY));
    ImGui::TextUnformatted(ScoreText);

    const char *Label = "Nickname";
    const ImVec2 LabelSize = ImGui::CalcTextSize(Label);

    const float InputWidth = 180.0f;
    const float LabelX = Layout.ContentLeft + (Layout.ContentWidth - LabelSize.x) * 0.5f;
    const float InputX = Layout.ContentLeft + (Layout.ContentWidth - InputWidth) * 0.5f;
    const float LabelY = ScoreY + 56.0f;
    const float InputY = LabelY + LabelSize.y + 12.0f;

    ImGui::SetCursorPos(ImVec2(LabelX, LabelY));
    ImGui::TextUnformatted(Label);

    ImGui::SetCursorPos(ImVec2(InputX, InputY));
    ImGui::PushItemWidth(InputWidth);
    if (ImGui::InputText("##NicknameInput", NicknameBuffer, sizeof(NicknameBuffer),
                         ImGuiInputTextFlags_CharsUppercase))
    {
        SyncNicknameFromBuffer();
    }
    ImGui::PopItemWidth();

    ImGui::SetWindowFontScale(1.0f);

    if (DrawBottomButton(Layout, "Save", 0, 2))
    {
        SyncNicknameFromBuffer();
        PendingAction = EUIPopupAction::ConfirmSaveScore;
    }

    if (DrawBottomButton(Layout, "Cancel", 1, 2))
    {
        PendingAction = EUIPopupAction::CancelSaveScore;
    }

    EndPopupWindow();
}

void FSaveScorePopup::SyncBufferFromNickname()
{
    for (int i = 0; i < MaxNicknameLength + 1; ++i)
        NicknameBuffer[i] = '\0';

    const int Length = static_cast<int>(Nickname.size());
    const int CopyCount = (Length < MaxNicknameLength) ? Length : MaxNicknameLength;

    for (int i = 0; i < CopyCount; ++i)
    {
        NicknameBuffer[i] = Nickname[i];
    }

    NicknameBuffer[CopyCount] = '\0';
}

void FSaveScorePopup::SyncNicknameFromBuffer()
{
    Nickname = NicknameBuffer;
}