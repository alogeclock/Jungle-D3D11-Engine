#include <array>
#include <string>
#include "GameContext.h"
#include "UIPopupBase.h"

// TODO
#define MAX_NICKNAME_LENGTH 6

#pragma once

enum class ENicknamePopupAction
{
    None,
    Submit,
    Cancel
};

struct FNicknamePopupResult
{
    ENicknamePopupAction Action = ENicknamePopupAction::None;
    std::string          Nickname;
};

class FNicknamePopup : public FUIPopupBase
{
  public:
    FNicknamePopup();

    void                 ResetInput();
    const char          *GetNickname() const;
    FNicknamePopupResult ConsumeResult();

    void Render(FGameContext &Context) override;

  private:
    std::array<char, 32> NicknameBuffer{};
    FNicknamePopupResult PendingResult{};
};

// =============================================================================

FNicknamePopup::FNicknamePopup() { ResetInput(); }

void FNicknamePopup::ResetInput() { NicknameBuffer.fill('\0'); }

const char *FNicknamePopup::GetNickname() const { return NicknameBuffer.data(); }

FNicknamePopupResult FNicknamePopup::ConsumeResult()
{
    FNicknamePopupResult Result = PendingResult;
    PendingResult = {};
    return Result;
}

void FNicknamePopup::Render(FGameContext &Context)
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
            PendingResult.Action = ENicknamePopupAction::Submit;
            PendingResult.Nickname = NicknameBuffer.data();

            ImGui::CloseCurrentPopup();
            Close();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            PendingResult.Action = ENicknamePopupAction::Cancel;
            PendingResult.Nickname.clear();

            ImGui::CloseCurrentPopup();
            Close();
        }

        ImGui::EndPopup();
    }
}