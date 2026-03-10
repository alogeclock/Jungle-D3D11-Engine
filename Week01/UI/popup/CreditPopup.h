#pragma once

#include <string>
#include <vector>
#include "UIPopupBase.h"

class FCreditPopup : public FUIPopupBase
{
  public:
    void SetCredits(const std::vector<std::string> &InCredits) { Credits = InCredits; }

    void Render(FGameContext &Context) override;
    void Update(FGameContext &Context) override {}

  private:
    std::vector<std::string> Credits = {"Test1", "Test2", "Test3"};
};

// =============================================================================

void FCreditPopup::Render(FGameContext &Context)
{
    if (ConsumeOpenRequest())
    {
        ImGui::OpenPopup("Credits");
    }

    if (!bIsOpen)
        return;

    if (ImGui::BeginPopupModal("Credits", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Credits");
        ImGui::Separator();

        for (const std::string &Name : Credits)
        {
            ImGui::BulletText("%s", Name.c_str());
        }

        if (ImGui::Button("Close", ImVec2(240, 0)))
        {
            ImGui::CloseCurrentPopup();
            Close();
        }

        ImGui::EndPopup();
    }
}
