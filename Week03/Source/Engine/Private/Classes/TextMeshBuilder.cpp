#include "Source/Engine/Public/Classes/TextMeshBuilder.h"
#include "Source/Engine/Public/Classes/TextLoader.h"
#include <iostream>
#include <string>
#include <sstream>

TMap<wchar_t, CharacterInfo> FTextMeshBuilder::charInfoMap;
void FTextMeshBuilder::InitializeCharInfo() {
    charInfoMap.clear();

    constexpr int   COLS   = 16;
    constexpr int   ROWS   = 16;
    constexpr float CELL_W = 1.0f / static_cast<float>(COLS);
    constexpr float CELL_H = 1.0f / static_cast<float>(ROWS);

    for (int i = 0; i < COLS * ROWS; ++i)
    {
        CharacterInfo Info;
        Info.u = (i % COLS) * CELL_W;
        Info.v = (i / COLS) * CELL_H;
        Info.width = CELL_W;
        Info.height = CELL_H;

        charInfoMap.insert({static_cast<wchar_t>(i), Info});
    }
    
    FString FntContent;
    if (UTextLoader::LoadTextFromFile("Data/Texture/Unnamed.txt", FntContent))
    {
        LoadFNT(FntContent, 256.f, 256.f);
    }
}

bool FTextMeshBuilder::bIsKorean(const FString& text)
{
    int32 i = 0;

    while (i < text.size())
    {
        unsigned char c = (unsigned char)text[i];
        uint32_t CP = 0;

        if (c < 0x80) // 1-byte ASCII
        {
            CP = c;
            i += 1;
        }
        else if ((c & 0xE0) == 0xC0) // 2-byte
        {
            if (i + 1 >= text.size()) break;

            CP = ((c & 0x1F) << 6) |
                 ((unsigned char)text[i + 1] & 0x3F);
            i += 2;
        }
        else if ((c & 0xF0) == 0xE0) // 3-byte
        {
            if (i + 2 >= text.size()) break;

            CP = ((c & 0x0F) << 12) |
                 (((unsigned char)text[i + 1] & 0x3F) << 6) |
                 ((unsigned char)text[i + 2] & 0x3F);
            i += 3;
        }
        else if ((c & 0xF8) == 0xF0) // 4-byte
        {
            if (i + 3 >= text.size()) break;

            CP = ((c & 0x07) << 18) |
                 (((unsigned char)text[i + 1] & 0x3F) << 12) |
                 (((unsigned char)text[i + 2] & 0x3F) << 6) |
                 ((unsigned char)text[i + 3] & 0x3F);
            i += 4;
        }
        else
        {
            ++i;
            continue;
        }

        if (CP >= 0xAC00 && CP <= 0xD7A3)
            return true;
    }

    return false;
}

const CharacterInfo *FTextMeshBuilder::GetCharInfo(wchar_t InChar)
{
    auto It = charInfoMap.find(InChar);
    if (It != charInfoMap.end()) return &It->second;
    return nullptr;
}

// UTF-8 한 글자 디코딩 + 인덱스 전진
static uint32_t DecodeUTF8(const FString& s, size_t& i)
{
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80) {
        return (uint32_t)s[i++];
    } else if (c < 0xE0) {
        uint32_t cp = (c & 0x1F) << 6 | ((unsigned char)s[i+1] & 0x3F);
        i += 2; return cp;
    } else if (c < 0xF0) {
        uint32_t cp = (c & 0x0F) << 12
                    | ((unsigned char)s[i+1] & 0x3F) << 6
                    | ((unsigned char)s[i+2] & 0x3F);
        i += 3; return cp;
    } else {
        uint32_t cp = (c & 0x07) << 18
                    | ((unsigned char)s[i+1] & 0x3F) << 12
                    | ((unsigned char)s[i+2] & 0x3F) << 6
                    | ((unsigned char)s[i+3] & 0x3F);
        i += 4; return cp;
    }
}

void FTextMeshBuilder::BuildTextMesh(const FString& InText, TArray<FTextureVertex>* Vertices, TArray<uint32>* Indices)
{
    if (charInfoMap.empty()) InitializeCharInfo();
    Vertices->clear();
    Indices->clear();

    constexpr float GlyphWidth  = 0.5f;
    constexpr float GlyphHeight = 0.5f;

    // 1. 글자 수 계산
    uint32 CharCount = 0;
    for (size_t i = 0; i < InText.size();) { DecodeUTF8(InText, i); CharCount++; }

    float PenX = -(static_cast<float>(CharCount) * GlyphWidth) * 0.5f;

    // 2. 메시 빌드
    for (size_t i = 0; i < InText.size();)
    {
        uint32_t CP = DecodeUTF8(InText, i);

        const CharacterInfo* Info = (CP == L' ') ? nullptr : GetCharInfo(static_cast<wchar_t>(CP));
        if (!Info) { PenX += GlyphWidth; continue; }

        const float x0 = PenX, x1 = PenX + GlyphWidth;
        const float y0 = -GlyphHeight * 0.5f, y1 = GlyphHeight * 0.5f;

        uint32 Base = static_cast<uint32>(Vertices->size());
        Vertices->push_back({ FVector(x0, y0, 0.f), Info->u,              Info->v               });
        Vertices->push_back({ FVector(x1, y0, 0.f), Info->u + Info->width, Info->v               });
        Vertices->push_back({ FVector(x0, y1, 0.f), Info->u,              Info->v + Info->height });
        Vertices->push_back({ FVector(x1, y1, 0.f), Info->u + Info->width, Info->v + Info->height });

        for (uint32 idx : { Base, Base+1, Base+2, Base+1, Base+3, Base+2 })
            Indices->push_back(idx);

        PenX += GlyphWidth;
    }
}

void FTextMeshBuilder::LoadFNT(const FString& FntContent, float AtlasW, float AtlasH)
{
    std::istringstream Stream(FntContent);
    std::string Line;

    while (std::getline(Stream, Line))
    {
        if (Line.find("char id=") == std::string::npos) continue;

        int id = 0, x = 0, y = 0, w = 0, h = 0;
        int xoff = 0, yoff = 0, xadv = 0;

        sscanf_s(Line.c_str(),
            "char id=%d x=%d y=%d width=%d height=%d xoffset=%d yoffset=%d xadvance=%d",
            &id, &x, &y, &w, &h, &xoff, &yoff, &xadv);

        CharacterInfo Info;
        Info.u      = (float)x / AtlasW;
        Info.v      = (float)y / AtlasH;
        Info.width  = (float)w / AtlasW;
        Info.height = (float)h / AtlasH;
        Info.bIsKorean = true;

        charInfoMap.insert({static_cast<wchar_t>(id), Info});
    }
}