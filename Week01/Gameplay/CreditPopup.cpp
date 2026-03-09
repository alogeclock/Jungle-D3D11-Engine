#include "CreditPopup.h"
#include "../Core/GameContext.h"
#include "../imgui/imgui.h"

void FCreditPopup::Open() { bIsOpen = true; }

void FCreditPopup::Close() { bIsOpen = false; }

bool FCreditPopup::IsOpen() const { return bIsOpen; }

void FCreditPopup::Update(FGameContext &Context) {}

void FCreditPopup::Render(FGameContext &Context)
{
    if (!bIsOpen)
        return; // ImGui 창 시작

    if (ImGui::Begin("Credits", &bIsOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("--- Credits ---");
        ImGui::Separator();
        ImGui::Text("코어 + 파일 처리: 김연하");
        ImGui::Text("렌더/UI: 이호진");
        ImGui::Text("게임플레이: 전현길, 심우진");

        ImGui::End();
    }
}
