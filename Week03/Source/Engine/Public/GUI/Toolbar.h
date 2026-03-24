#pragma once
#include "CoreTypes.h"
#include "Source/Engine/Public/GUI/GuiWindow.h"

class FToolbar
{
    TArray<FGuiWindow*> Elements;
    ImVec2 ToolbarElementSize;

public:
    FToolbar();
    FGuiMsg ShowToolbar(ImVec2 InPos, ImVec2 InSize);
};