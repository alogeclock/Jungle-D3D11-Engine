#include "pch.h"
#include "DebugWidget.h"
#include "Core/AudioSystem.h"
#include "Gameplay/BeatSystem.h"
#include "Gameplay/Stage.h"

void FDebugWidget::BindStage(FStage *InStage)
{
    Stage = InStage;
}

void FDebugWidget::Update(FGameContext &Context)
{
}

void FDebugWidget::Render(FGameContext &Context)
{
#ifdef DEBUG
    if (!Stage)
        return;

    ImGui::SetNextWindowPos(ImVec2(10, 200), ImGuiCond_Once);
    ImGui::SetNextWindowBgAlpha(0.6f);

    if (ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::Checkbox("Invincible", &bInvincible))
        {
            Stage->GetPlayer().SetInvincible(bInvincible);
        }

        ImGui::Separator();
        ImGui::Text("BPM: %.1f", Stage->GetBeatSystem().GetBpm());
        if (ImGui::SliderFloat("TimeScale", &TimeScale, 0.2f, 2.0f, "%.2f"))
        {
            Stage->GetBeatSystem().SetTimeScale(TimeScale);
            FAudioSystem::Get().SetAllPlaybackRate(TimeScale);
        }
        if (ImGui::Button("Reset##TimeScale"))
        {
            TimeScale = 1.0f;
            Stage->GetBeatSystem().SetTimeScale(1.0f);
            FAudioSystem::Get().SetAllPlaybackRate(1.0f);
        }
    }
    ImGui::End();
#endif
}
