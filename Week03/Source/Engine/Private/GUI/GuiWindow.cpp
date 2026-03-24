#include "Source/Engine/Public/GUI/GuiWindow.h"

FGuiWindow::FGuiWindow(FString InName) : DisplayName(InName)
{
    bIsVisable = false;
}

FGuiMsg FGuiWindow::ShowPanel(ImVec2 Pos)
{
    ImGui::SetNextWindowPos(Pos);
    ImGui::PushID(this);
    ImGui::Begin(DisplayName.c_str(), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);

    FGuiMsg reulstMsg = ShowDetail();

    ImGui::End();
    ImGui::PopID();

    return reulstMsg;
}

bool FGuiWindow::SetVisible(bool InVisable)
{
    return bIsVisable = InVisable;
}

bool FGuiWindow::GetVisable()
{
    return bIsVisable;
}

FString FGuiWindow::GetName() const
{
    return DisplayName;
}
