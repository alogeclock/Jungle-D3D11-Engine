#pragma once

struct ImVec2;
class UClass;

class FSpawnGUI
{
    bool bIsVisible = false;
    int SpawnCount = 1;

public:
    bool GetVisible();
    void SetVisible(bool InVisible);
    bool IsInBound(ImVec2 InScreenMousePosition);
    void ShowDetail();
    void DrawContextMenu();

private:
    void SpawnPrimitiveComponent(UClass* ComponentClassToSpawn, int NumberOfSpawn);
};
