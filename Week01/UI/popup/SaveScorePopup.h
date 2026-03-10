#pragma once

#include <array>
#include <string>
#include "Core/GameContext.h"
#include "UIPopupBase.h"

// TODO
#define MAX_NICKNAME_LENGTH 6

enum class ESaveScorePopupAction
{
    None,
    Submit,
    Cancel
};

struct FSaveScorePopupResult
{
    ESaveScorePopupAction Action = ESaveScorePopupAction::None;
    std::string           Nickname;
};

class FSaveScorePopup : public FUIPopupBase
{
  public:
    FSaveScorePopup();

    void                  ResetInput();
    const char           *GetNickname() const;
    FSaveScorePopupResult ConsumeResult();

    void Render(FGameContext &Context) override;
    void Update(FGameContext &Context) override {}

  private:
    std::array<char, MAX_NICKNAME_LENGTH + 1> NicknameBuffer{};
    FSaveScorePopupResult                     PendingResult{};
};

// =============================================================================

FSaveScorePopup::FSaveScorePopup() { ResetInput(); }

void FSaveScorePopup::ResetInput() { NicknameBuffer.fill('\0'); }

const char *FSaveScorePopup::GetNickname() const { return NicknameBuffer.data(); }

FSaveScorePopupResult FSaveScorePopup::ConsumeResult()
{
    FSaveScorePopupResult Result = PendingResult;
    PendingResult = {};
    return Result;
}

void FSaveScorePopup::Render(FGameContext &Context)
{
    if (ConsumeOpenRequest())
    {
        ImGui::OpenPopup("Save Score");
    }

    if (!bIsOpen)
        return;

    if (ImGui::BeginPopupModal("Save Score", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("All stages cleared!");
        ImGui::Separator();
        ImGui::Text("Enter your nickname.");

        ImGui::InputText("Nickname", NicknameBuffer.data(), NicknameBuffer.size());

        const bool bCanSubmit = NicknameBuffer[0] != '\0';

        if (!bCanSubmit)
        {
            ImGui::TextDisabled("Nickname is required.");
        }

        if (ImGui::Button("Save", ImVec2(120, 0)) && bCanSubmit)
        {
            PendingResult.Action = ESaveScorePopupAction::Submit;
            PendingResult.Nickname = NicknameBuffer.data();

            ImGui::CloseCurrentPopup();
            Close();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            PendingResult.Action = ESaveScorePopupAction::Cancel;
            PendingResult.Nickname.clear();

            ImGui::CloseCurrentPopup();
            Close();
        }

        ImGui::EndPopup();
    }
}
