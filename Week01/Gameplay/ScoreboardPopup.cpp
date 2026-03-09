#include "ScoreboardPopup.h"
#include "../Core/GameContext.h"
#include "../imgui/imgui.h"

void FScoreboardPopup::SetScores(const std::vector<FScoreRecord> &InScores) { Scores = InScores; }

void FScoreboardPopup::Open() { bIsOpen = true; }

void FScoreboardPopup::Close() { bIsOpen = false; }

bool FScoreboardPopup::IsOpen() const { return bIsOpen; }

void FScoreboardPopup::Update(FGameContext &Context) {}

void FScoreboardPopup::Render(FGameContext &Context)
{
    if (!bIsOpen)
        return; // ImGui 창 시작

    if (ImGui::Begin("Scoreboard", &bIsOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("--- High Scores ---");
        ImGui::Separator();

        // 저장된 점수 기록들 출력
        for (size_t i = 0; i < Scores.size(); ++i)
        {
            ImGui::Text("%d. %s : %d", static_cast<int>(i + 1), Scores[i].Name.c_str(),
                        Scores[i].Score);
        }

        if (Scores.empty())
        {
            ImGui::Text("No records found.");
        }

        ImGui::Separator();

        // 하단 닫기 버튼
        if (ImGui::Button("Close", ImVec2(120, 0)))
        {
            Close();
        }

        ImGui::End();
    }
}
