#pragma once
#include "CoreTypes.h"
#include "ImGui/imgui.h"


class FSceneWindowTab
{
    TMap<FString, std::wstring> NameToPath;
    std::wstring CurrentPath;

public:
    FSceneWindowTab();
    void LoadFilePath();
    void ShowWindow();
};