#pragma once

struct ImVec2;
class FSpawnGUI
{
    bool bIsVisible = false;

public:
    bool GetVisible();
    void SetVisible(bool InVisible);
    bool IsInBound(ImVec2 InScreenMousePosition);
    void ShowDetail();
};