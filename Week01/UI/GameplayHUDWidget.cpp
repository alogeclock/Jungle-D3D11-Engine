#include "pch.h"
#include "GameplayHUDWidget.h"
#include "Gameplay/Stage.h"
#include "Core/Time.h"
#include "imgui/imgui.h"

void FGameplayHUDWidget::BindStage(const FStage* InStage)
{
	Stage = InStage;
}

void FGameplayHUDWidget::ResetPlayTime()
{
	PlayTime = 0.0f;
}

void FGameplayHUDWidget::Update(FGameContext& Context)
{
	PlayTime += Context.Time.GetDeltaTime();
}

void FGameplayHUDWidget::Render(FGameContext& Context)
{
	if (!Stage) return;

	int Score = Stage->GetScoreSystem().GetScore();
	int Combo = Stage->GetScoreSystem().GetCombo();
	int HP = Stage->GetPlayer().GetHp();

	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.5f);

	ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration
		| ImGuiWindowFlags_NoInputs
		| ImGuiWindowFlags_AlwaysAutoResize
		| ImGuiWindowFlags_NoFocusOnAppearing
		| ImGuiWindowFlags_NoNav;

	if (ImGui::Begin("##HUD", nullptr, Flags))
	{
		int Minutes = static_cast<int>(PlayTime) / 60;
		int Seconds = static_cast<int>(PlayTime) % 60;

		ImGui::Text("HP    %d", HP);
		ImGui::Text("Time  %d:%02d", Minutes, Seconds);
		ImGui::Text("Score %d", Score);
		if (Combo > 0)
		{
			ImGui::Text("Combo x%d", Combo);
		}
	}
	ImGui::End();
}
