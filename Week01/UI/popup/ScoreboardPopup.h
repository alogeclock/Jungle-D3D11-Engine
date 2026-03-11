#pragma once

#include <vector>
#include "Data/ScoreRecord.h"
#include "UIPopupBase.h"

class FScoreboardPopup : public FUIPopupBase
{
  public:
    void SetRecords(const std::vector<FScoreRecord> &InRecords) { Records = InRecords; }

    void Render(FGameContext &Context) override;
    void Update(FGameContext &Context) override {}

  private:
    std::vector<FScoreRecord> Records;
};

// =============================================================================

void FScoreboardPopup::Render(FGameContext &Context)
{
    if (ConsumeOpenRequest())
    {
        ImGui::OpenPopup("Scoreboard");
    }

    if (!bIsOpen)
        return;

    if (ImGui::BeginPopupModal("Scoreboard", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Scoreboard");
        ImGui::Separator();

        if (ImGui::BeginTable("ScoreTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Nickname");
            ImGui::TableSetupColumn("Stage");
            ImGui::TableSetupColumn("Score");
            ImGui::TableHeadersRow();

            for (const FScoreRecord &Record : Records)
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", Record.Name.c_str());

                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%d", Record.Stage);

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%d", Record.Score);
            }

            ImGui::EndTable();
        }

        if (ImGui::Button("Close", ImVec2(300, 0)))
        {
            ImGui::CloseCurrentPopup();
            Close();
        }

        ImGui::EndPopup();
    }
}