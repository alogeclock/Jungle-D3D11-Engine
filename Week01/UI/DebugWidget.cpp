#include "pch.h"
#include "DebugWidget.h"
#include "Gameplay/Stage.h"
#include "imgui/imgui.h"

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
    }
    ImGui::End();
#endif
}
