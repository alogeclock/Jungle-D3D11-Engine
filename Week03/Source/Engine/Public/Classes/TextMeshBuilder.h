#pragma once

#include "Source/Engine/Public/Classes/Components/TextComponent.h"

struct CharacterInfo
{
    float u;      // UV 시작 X (0~1)
    float v;      // UV 시작 Y (0~1)
    float width;  // UV 폭
    float height; // UV 높이
    bool bIsKorean = false;
};

class FTextMeshBuilder
{
public:
  static constexpr uint32 MAX_FONT_VERTICES = 4096;

  static void InitializeCharInfo();
  static const CharacterInfo *GetCharInfo(wchar_t InChar);
  static TArray<FTextVertex> BuildTextMesh(const FString& text);

private:
  static TMap<wchar_t, CharacterInfo> charInfoMap;
  static void LoadFNT(const FString& FntContent, float AtlasW, float AtlasH);
};
