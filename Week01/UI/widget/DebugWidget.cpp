#include "pch.h"
#include "DebugWidget.h"
#include "Core/AudioSystem.h"
#include "Gameplay/BeatSystem.h"
#include "Gameplay/Item.h"
#include "Gameplay/Stage.h"

void FDebugWidget::BindStage(FStage* InStage)
{
	Stage = InStage;
	if (Stage)
	{
		DarknessLevel = Stage->GetDarknessLevel();
		bTimeFrozen = Stage->IsTimeFrozen();
	}
}

void FDebugWidget::BindPauseFlag(bool* InPauseFlag) { PauseFlag = InPauseFlag; }

void FDebugWidget::SetStageChangeCallback(std::function<void(int)> Callback)
{
	OnStageChange = std::move(Callback);
}

void FDebugWidget::SetTotalStages(int Count) { TotalStages = Count; }

void FDebugWidget::SetTextures(FGameContext& Context) {}

void FDebugWidget::Update(FGameContext& Context)
{
}

void FDebugWidget::Render(FGameContext& Context)
{
#ifdef DEBUG
	if (!Stage)
		return;

	ImGui::SetNextWindowPos(ImVec2(10, 200), ImGuiCond_Once);
	ImGui::SetNextWindowBgAlpha(0.6f);

	if (ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		// --- Pause ---
		if (ImGui::Checkbox("Pause", &bDebugPaused))
		{
			if (PauseFlag)
				*PauseFlag = bDebugPaused;
		}

		ImGui::Separator();
		// --- Cheats (체크박스) ---
		if (ImGui::Checkbox("Invincible", &bInvincible))
		{
			Stage->GetPlayer().SetInvincible(bInvincible);
		}
		if (ImGui::Checkbox("Freeze Time", &bTimeFrozen))
		{
			Stage->SetTimeFrozen(bTimeFrozen);
		}
		if (ImGui::Checkbox("Disable Darkness", &bDarknessDisabled))
		{
			Stage->SetDarknessDisabled(bDarknessDisabled);
		}

		// --- Settings ---
		ImGui::Separator();
		ImGui::Text("BPM: %.1f", Stage->GetBeatSystem().GetBpm());
		if (ImGui::SliderFloat("TimeScale", &TimeScale, 0.2f, 2.0f, "%.2f"))
		{
			Stage->GetBeatSystem().SetTimeScale(TimeScale);
			FAudioSystem::Get().SetAllPlaybackRate(TimeScale);
		}

        DarknessLevel = Stage->GetDarknessLevel();
        int DisplayLevel = DarknessLevel + 1;
        if (ImGui::SliderInt("Darkness", &DisplayLevel, 1, 5))
        {
            DarknessLevel = DisplayLevel - 1;
            Stage->SetDarknessLevel(DarknessLevel);
        }

        // --- Item Cheats ---
        ImGui::Separator();
        ImGui::Text("Item Cheats");

        if (ImGui::Button("Invincibility"))
        {
            FItemData Item;
            Item.Type = EItemType::Invincibility;
            Stage->ApplyItem(Item);
        }
        ImGui::SameLine();
        if (ImGui::Button("Time Freeze"))
        {
            FItemData Item;
            Item.Type = EItemType::TimeFreeze;
            Item.Duration = 5.0f;
            Stage->ApplyItem(Item);
        }

        if (ImGui::Button("TimeScale Up"))
        {
            FItemData Item;
            Item.Type = EItemType::TimeScaleUp;
            Item.Duration = 6.0f;
            Stage->ApplyItem(Item);
        }
        ImGui::SameLine();
        if (ImGui::Button("TimeScale Down"))
        {
            FItemData Item;
            Item.Type = EItemType::TimeScaleDown;
            Item.Duration = 6.0f;
            Stage->ApplyItem(Item);
        }

        if (ImGui::Button("Darkness Up"))
        {
            FItemData Item;
            Item.Type = EItemType::DarknessUp;
            Stage->ApplyItem(Item);
            DarknessLevel = Stage->GetDarknessLevel();
        }
        ImGui::SameLine();
        if (ImGui::Button("Darkness Down"))
        {
            FItemData Item;
            Item.Type = EItemType::DarknessDown;
            Stage->ApplyItem(Item);
            DarknessLevel = Stage->GetDarknessLevel();
        }

        // Active Effects 표시
        const auto& PlayerEffects = Stage->GetPlayer().GetActiveEffects();
        float FreezeRemaining = Stage->GetTimeFreezeRemaining();
        bool bHasEffects = !PlayerEffects.empty() || Stage->IsTimeFrozen();
        if (bHasEffects)
        {
            ImGui::Text("Active: ");
            for (const auto& Eff : PlayerEffects)
            {
                switch (Eff.Type)
                {
                case EItemType::Invincibility:  ImGui::SameLine(); ImGui::TextColored(ImVec4(1,1,0,1), "[Invincible]"); break;
                case EItemType::TimeScaleUp:    ImGui::SameLine(); ImGui::TextColored(ImVec4(1,0.5f,0,1), "[Speed+] %.1fs", Eff.RemainingTime); break;
                case EItemType::TimeScaleDown:  ImGui::SameLine(); ImGui::TextColored(ImVec4(0,0.8f,1,1), "[Speed-] %.1fs", Eff.RemainingTime); break;
                default: break;
                }
            }
            if (Stage->IsTimeFrozen())
            {
                ImGui::SameLine(); ImGui::TextColored(ImVec4(0,1,1,1), "[Freeze] %.1fs", FreezeRemaining);
            }
        }

        // --- Reset All ---
        ImGui::Separator();
        if (ImGui::Button("Reset All"))
        {
            bInvincible = false;
            Stage->GetPlayer().SetInvincible(false);

			bTimeFrozen = false;
			Stage->SetTimeFrozen(false);

			bDarknessDisabled = false;
			Stage->SetDarknessDisabled(false);

			TimeScale = 1.0f;
			Stage->GetBeatSystem().SetTimeScale(1.0f);
			FAudioSystem::Get().SetAllPlaybackRate(1.0f);

			DarknessLevel = 2;
			Stage->SetDarknessLevel(2);
		}

		// --- Stage Select ---
		if (OnStageChange)
		{
			ImGui::Separator();
			ImGui::Text("Stage");
			int CurrentStage = Stage->GetCurrentStageIndex();
			for (int i = 0; i < TotalStages; ++i)
			{
				ImGui::SameLine();
				char Label[16];
				snprintf(Label, sizeof(Label), "%d", i + 1);
				bool bCurrent = (i == CurrentStage);
				if (bCurrent) ImGui::BeginDisabled();
				if (ImGui::Button(Label, ImVec2(30, 0)))
				{
					OnStageChange(i);
				}
				if (bCurrent) ImGui::EndDisabled();
			}
		}
	}
	ImGui::End();
#endif
}
