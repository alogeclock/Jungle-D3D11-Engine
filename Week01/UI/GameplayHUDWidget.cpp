#include "pch.h"
#include "GameplayHUDWidget.h"
#include "Gameplay/Stage.h"
#include "Core/Time.h"
#include "Render/Renderer.h"
#include "imgui/imgui.h"

void FGameplayHUDWidget::BindStage(const FStage* InStage)
{
	Stage = InStage;
}

void FGameplayHUDWidget::BindPauseFlag(bool *InPauseFlag)
{
	PauseFlag = InPauseFlag;
}

void FGameplayHUDWidget::ResetPlayTime()
{
	PlayTime = 0.0f;
}

void FGameplayHUDWidget::Update(FGameContext& Context)
{
	bool bPaused = PauseFlag && *PauseFlag;
	if (!bPaused)
	{
		PlayTime += Context.Time.GetDeltaTime();
	}
}

void FGameplayHUDWidget::Render(FGameContext& Context)
{
	if (!Stage) return;

	bool bPaused = PauseFlag && *PauseFlag;

	int Score = Stage->GetScoreSystem().GetScore();
	int Combo = Stage->GetScoreSystem().GetCombo();
	int HP = Stage->GetPlayer().GetHp();

	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.5f);

	ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration
		| ImGuiWindowFlags_AlwaysAutoResize
		| ImGuiWindowFlags_NoFocusOnAppearing
		| ImGuiWindowFlags_NoNav;

	if (ImGui::Begin("##HUD", nullptr, Flags))
	{
		float Remaining = Stage->GetRemainingTime();
		int Minutes = static_cast<int>(Remaining) / 60;
		int Seconds = static_cast<int>(Remaining) % 60;

		ImGui::Text("HP    %d", HP);
		if (Remaining <= 10.0f)
			ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "Time  %d:%02d", Minutes, Seconds);
		else
			ImGui::Text("Time  %d:%02d", Minutes, Seconds);
		ImGui::Text("Score %d", Score);
		if (Combo > 0)
		{
			ImGui::Text("Combo x%d", Combo);
		}

		ImGui::Separator();
		if (PauseFlag)
		{
			if (ImGui::Button(bPaused ? "Resume" : "Pause", ImVec2(80, 0)))
			{
				*PauseFlag = !*PauseFlag;
			}
		}
	}
	ImGui::End();

	// 일시정지 오버레이
	if (bPaused)
	{
		float ScreenW = static_cast<float>(Context.Renderer.GetScreenWidth());
		float ScreenH = static_cast<float>(Context.Renderer.GetScreenHeight());

		ImGui::SetNextWindowPos(ImVec2(ScreenW * 0.5f, ScreenH * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowBgAlpha(0.7f);

		ImGuiWindowFlags PauseFlags = ImGuiWindowFlags_NoDecoration
			| ImGuiWindowFlags_AlwaysAutoResize
			| ImGuiWindowFlags_NoNav;

		if (ImGui::Begin("##PauseOverlay", nullptr, PauseFlags))
		{
			ImGui::Text("PAUSED");
			ImGui::Separator();
			if (ImGui::Button("Resume", ImVec2(120, 0)))
			{
				*PauseFlag = false;
			}
		}
		ImGui::End();
	}
}
