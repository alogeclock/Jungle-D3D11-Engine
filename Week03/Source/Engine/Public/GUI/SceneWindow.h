#pragma once
#include "Source/Engine/Public/GUI/GuiWindow.h"

class FSceneWindow : public FGuiWindow
{
    char buffer[256] = {}; // 반드시 0 초기화

    std::wstring SaveFileDialog();
    std::wstring OpenFileDialog();
    FString WStringToFString(const std::wstring& string);
    std::wstring CharToWString(const char* msg);

public:
    FSceneWindow(FString InName);
    virtual FGuiMsg ShowDetail() override;
};