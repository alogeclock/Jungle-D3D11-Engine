#include "CreditPopup.h"
#include "Core/GameContext.h"
#include "imgui/imgui.h"

void FCreditPopup::Open() { bIsOpen = true; }

void FCreditPopup::Close() { bIsOpen = false; }

bool FCreditPopup::IsOpen() const { return bIsOpen; }

void FCreditPopup::Update(FGameContext& Context) {}

void FCreditPopup::Render(FGameContext& Context)
{
	if (!bIsOpen)
		return;

	if (ImGui::Begin("Credits", &bIsOpen, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("--- Credits ---");
		ImGui::Separator();
		ImGui::Text("Core + File : YeonHa Kim");
		ImGui::Text("Render/UI: Hojin Lee");
		ImGui::Text("Game Play: Jeon Heon gil, Sim Woo jin");

		ImGui::End();
	}
}
