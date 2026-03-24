#pragma once
#include "ImGui/imgui.h"
#include "Source/Engine/Public/GUI/GuiMsg.h"

class FGuiWindow
{
    FString DisplayName;
    bool bIsVisable;

protected:
    virtual FGuiMsg ShowDetail() = 0;

public:
    FGuiWindow(FString InName);
    FGuiMsg ShowPanel(ImVec2 Pos);
    bool SetVisible(bool InVisable);
    bool GetVisable();
    FString GetName() const;
};