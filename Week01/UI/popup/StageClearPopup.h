#pragma once

#include "UIPopupBase.h"

enum class EStageClearPopupAction
{
    None,
    NextStage,
    Close
};

class FStageClearPopup : public FUIPopupBase
{
  public:
    void SetData(bool bInAllCleared, int InClearedStage)
    {
        bAllCleared = bInAllCleared;
        ClearedStage = InClearedStage;
    }

    EStageClearPopupAction ConsumeAction();

    void Render(FGameContext &Context) override;

  private:
    bool                   bAllCleared = false;
    int                    ClearedStage = 0;
    EStageClearPopupAction PendingAction = EStageClearPopupAction::None;
};

// =============================================================================

EStageClearPopupAction FStageClearPopup::ConsumeAction()
{
    EStageClearPopupAction Result = PendingAction;
    PendingAction = EStageClearPopupAction::None;
    return Result;
}

void FStageClearPopup::Render(FGameContext &Context)
{
    if (ConsumeOpenRequest())
    {
        ImGui::OpenPopup("Stage Clear");
    }

    if (!bIsOpen)
        return;

    const ImGuiWindowFlags Flags = ImGuiWindowFlags_AlwaysAutoResize;
    if (ImGui::BeginPopupModal("Stage Clear", nullptr, Flags))
    {
        ImGui::Text("Stage %d Clear!", ClearedStage);

        if (bAllCleared)
        {
            ImGui::Separator();
            ImGui::Text("You cleared all stages!");
            ImGui::Text("Please enter your nickname for score saving.");
        }
        else
        {
            ImGui::Separator();
            ImGui::Text("Go to next stage?");
        }

        if (!bAllCleared)
        {
            if (ImGui::Button("Next Stage", ImVec2(140, 0)))
            {
                PendingAction = EStageClearPopupAction::NextStage;
                ImGui::CloseCurrentPopup();
                Close();
            }

            ImGui::SameLine();

            if (ImGui::Button("Close", ImVec2(140, 0)))
            {
                PendingAction = EStageClearPopupAction::Close;
                ImGui::CloseCurrentPopup();
                Close();
            }
        }
        else
        {
            if (ImGui::Button("OK", ImVec2(280, 0)))
            {
                PendingAction = EStageClearPopupAction::Close;
                ImGui::CloseCurrentPopup();
                Close();
            }
        }

        ImGui::EndPopup();
    }
}
