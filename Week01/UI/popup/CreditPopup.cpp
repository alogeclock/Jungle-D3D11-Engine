#include "pch.h"
#include "CreditPopup.h"

EUIPopupAction FCreditPopup::ConsumeAction()
{
    EUIPopupAction Result = PendingAction;
    PendingAction = EUIPopupAction::None;

    return Result;
}

void FCreditPopup::Render(FGameContext &Context)
{
    FPopupFrameLayout Layout;
    if (!BeginPopupWindow("Credits", "Credits", ImVec2(DefaultPopupWidth, DefaultPopupHeight),
                          Layout))
    {
        return;
    }

    DrawCredits(Layout);

    if (DrawBottomButton(Layout, "Close"))
    {
        PendingAction = EUIPopupAction::ClosePopup;
    }

    EndPopupWindow();
}

void FCreditPopup::DrawCredits(const FPopupFrameLayout &Layout)
{
    ImGui::SetWindowFontScale(GetContentFontScale(ContentTextSize));

    const float LineHeight = ImGui::GetTextLineHeight();
    float BlockHeight = 0.0f;

    if (!Credits.empty())
    {
        BlockHeight = static_cast<float>(Credits.size()) * LineHeight +
                      static_cast<float>(Credits.size() - 1) * LineGap;
    }

    float StartY = Layout.ContentTop + (Layout.ContentHeight - BlockHeight) * 0.5f;
    if (StartY < Layout.ContentTop)
        StartY = Layout.ContentTop;

    const char *PipeText = "|";
    const ImVec2 PipeSize = ImGui::CalcTextSize(PipeText);

    for (std::size_t i = 0; i < Credits.size(); ++i)
    {
        const FCreditEntry &Entry = Credits[i];

        const ImVec2 NameSize = ImGui::CalcTextSize(Entry.Name.c_str());
        const ImVec2 RoleSize = ImGui::CalcTextSize(Entry.Role.c_str());

        const float TotalWidth = NameSize.x + PipeGap + PipeSize.x + PipeGap + RoleSize.x;
        const float BaseX = GetAlignedX(Layout, TotalWidth, ContentAlign) + BlockOffsetX;
        const float Y = StartY + static_cast<float>(i) * (LineHeight + LineGap);

        const float NameX = BaseX;
        const float PipeX = NameX + NameSize.x + PipeGap;
        const float RoleX = PipeX + PipeSize.x + PipeGap;

        ImGui::SetCursorPos(ImVec2(NameX, Y));
        ImGui::TextUnformatted(Entry.Name.c_str());

        ImGui::SetCursorPos(ImVec2(PipeX, Y));
        ImGui::TextUnformatted(PipeText);

        ImGui::SetCursorPos(ImVec2(RoleX, Y));
        ImGui::TextUnformatted(Entry.Role.c_str());
    }

    ImGui::SetWindowFontScale(1.0f);
}