#pragma once
#include "CoreTypes.h"
#include "Source/Engine/Public/GUI/GuiMsg.h"

class FScenemanager
{
    char buffer[256] = {}; // 반드시 0 초기화
    std::wstring CharToWString(const char* msg);
    FString WStringToFString(const std::wstring& string);


public:
    FScenemanager();
    void LoadScene(const std::wstring& InFilePath);
    void SaveScene(const std::wstring& InFilePath);
};

inline FScenemanager GSceneManager;