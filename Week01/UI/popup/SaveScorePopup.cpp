#include "pch.h"
#include "SaveScorePopup.h"
#include <cctype>
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
    char StageText[64]{};
    std::snprintf(ScoreText, sizeof(ScoreText), "Score : %d", Score);
    std::snprintf(StageText, sizeof(StageText), "Stage : %d", Stage);

    ImGui::SetWindowFontScale(1.0f);

    const ImVec2 ScoreSize = CalcUtf8TextSize(ScoreText);
    const ImVec2 StageSize = CalcUtf8TextSize(StageText);

    const float ScoreX = GetAlignedX(Layout, ScoreSize.x, EUIPopupContentAlign::Center);
    const float StageX = GetAlignedX(Layout, StageSize.x, EUIPopupContentAlign::Center);

    const float ScoreY = Layout.ContentTop + 20.0f;
    const float StageY = ScoreY + ScoreSize.y + 10.0f;

    ImGui::SetCursorPos(ImVec2(ScoreX, ScoreY));
    TextUnformattedUtf8(ScoreText);

    ImGui::SetCursorPos(ImVec2(StageX, StageY));
    TextUnformattedUtf8(StageText);

    const char *Label = "Nickname";
    const ImVec2 LabelSize = CalcUtf8TextSize(Label);

    const float InputWidth = 180.0f;
    const float LabelX = Layout.ContentLeft + (Layout.ContentWidth - LabelSize.x) * 0.5f;
    const float InputX = Layout.ContentLeft + (Layout.ContentWidth - InputWidth) * 0.5f;
    const float LabelY = StageY + StageSize.y + 20.0f;
    const float InputY = LabelY + LabelSize.y + 12.0f;

    ImGui::SetCursorPos(ImVec2(LabelX, LabelY));
    TextUnformattedUtf8(Label);

    ImGui::SetCursorPos(ImVec2(InputX, InputY));
    ImGui::PushItemWidth(InputWidth);
    if (ImGui::InputText("##NicknameInput", NicknameBuffer, sizeof(NicknameBuffer),
                         ImGuiInputTextFlags_CharsUppercase))
    {
        SyncNicknameFromBuffer();

        if (IsValidNickname())
            bShowValidationMessage = false;
    }
    ImGui::PopItemWidth();

    if (bShowValidationMessage)
    {
        const char *ValidationText = "Letters only, up to 6 characters, cannot be empty.";
        const ImVec2 ValidationSize = CalcUtf8TextSize(ValidationText);
        const float ValidationX =
            Layout.ContentLeft + (Layout.ContentWidth - ValidationSize.x) * 0.5f;

        const float ValidationY = Layout.ContentTop + Layout.ContentHeight - 36.0f;

        ImGui::SetCursorPos(ImVec2(ValidationX, ValidationY));
        TextUnformattedUtf8(ValidationText);
    }

    if (DrawBottomButton(Layout, "Save", 0, 2))
    {
        SyncNicknameFromBuffer();

        if (IsValidNickname())
        {
            bShowValidationMessage = false;
            PendingAction = EUIPopupAction::ConfirmSaveScore;
        }
        else
        {
            bShowValidationMessage = true;
        }
    }

    if (DrawBottomButton(Layout, "Cancel", 1, 2))
    {
        bShowValidationMessage = false;
        PendingAction = EUIPopupAction::OpenGoToTitlePopup;
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

bool FSaveScorePopup::IsValidNickname() const
{
    if (Nickname.empty())
        return false;

    const int Length = static_cast<int>(Nickname.size());
    if (Length > MaxNicknameLength)
        return false;

    for (int i = 0; i < Length; ++i)
    {
        const unsigned char Ch = static_cast<unsigned char>(Nickname[i]);
        if (!std::isalpha(Ch))
            return false;
    }

    return true;
}