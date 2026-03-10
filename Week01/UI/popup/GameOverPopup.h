#pragma once

#include "UIPopupBase.h"

enum class EGameOverPopupAction
{
    None,
    BackToTitle,
    Close
};

class FGameOverPopup : public FUIPopupBase
{
  public:
    EGameOverPopupAction ConsumeAction();
    void                 Render(FGameContext &Context) override;

  private:
    EGameOverPopupAction PendingAction = EGameOverPopupAction::None;
};

// =============================================================================

EGameOverPopupAction FGameOverPopup::ConsumeAction()
{
    EGameOverPopupAction Result = PendingAction;
    PendingAction = EGameOverPopupAction::None;
    return Result;
}

void FGameOverPopup::Render(FGameContext &Context)
{
    if (ConsumeOpenRequest())
    {
        ImGui::OpenPopup("Game Over");
    }

    if (!bIsOpen)
        return;

    if (ImGui::BeginPopupModal("Game Over", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Game Over");
        ImGui::Separator();
        ImGui::Text("Return to title?");

        if (ImGui::Button("Back to Title", ImVec2(200, 0)))
        {
            PendingAction = EGameOverPopupAction::BackToTitle;
            ImGui::CloseCurrentPopup();
            Close();
        }

        ImGui::SameLine();

        if (ImGui::Button("Close", ImVec2(120, 0)))
        {
            PendingAction = EGameOverPopupAction::Close;
            ImGui::CloseCurrentPopup();
            Close();
        }

        ImGui::EndPopup();
    }
}
