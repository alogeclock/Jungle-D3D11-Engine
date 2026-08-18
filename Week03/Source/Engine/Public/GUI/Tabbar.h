#pragma once
#include "CoreTypes.h"
#include "Source/Engine/Public/GUI/GuiWindow.h"

class FTabbar
{
    TArray<FGuiWindow*> Elements;
    ImVec2 ToolbarElementSize;

public:
    FTabbar();
    FGuiMsg ShowTabbar(ImVec2 InPos, ImVec2 InSize);
};