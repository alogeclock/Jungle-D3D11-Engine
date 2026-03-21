#include "Source/Engine/Public/Classes/TextMeshBuilder.h"
#include "Source/Engine/Public/Classes/TextLoader.h"
#include <iostream>

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
    if (UTextLoader::LoadTextFromFile("Data/Texture/KorName.txt", FntContent))
    {
        LoadFNT(FntContent, 256.f, 256.f);
        
        // ← FNT 로드 확인
        char buf[64];
        sprintf_s(buf, "charInfoMap size: %zu\n", charInfoMap.size());
        OutputDebugStringA(buf);
    }
    else
    {
        OutputDebugStringA("FNT 로드 실패\n"); // ← 이게 뜨면 경로 문제
    }
}

const CharacterInfo *FTextMeshBuilder::GetCharInfo(wchar_t InChar)
{
    auto It = charInfoMap.find(InChar);
    if (It != charInfoMap.end()) return &It->second;
    return nullptr;
}

TArray<FTextVertex> FTextMeshBuilder::BuildTextMesh(const FString& InText)
{
    if (charInfoMap.empty()) InitializeCharInfo();

    TArray<FTextVertex> Vertices;
    Vertices.reserve(InText.size() * 6);

    const float GlyphWidth  = 0.5f;
    const float GlyphHeight = 0.5f;
    const float TotalWidth  = static_cast<float>(InText.size()) * GlyphWidth;
    float PenX = -TotalWidth * 0.5f;

    size_t i = 0;
    while (i < InText.size())
    {
        // UTF-8 → 코드포인트 디코딩
        uint32_t CP = 0;
        unsigned char c = (unsigned char)InText[i];

        if      (c < 0x80) { CP = c;                                                                                                    i += 1; }
        else if (c < 0xE0) { CP = (c & 0x1F) << 6  | ((unsigned char)InText[i+1] & 0x3F);                                              i += 2; }
        else if (c < 0xF0) { CP = (c & 0x0F) << 12 | ((unsigned char)InText[i+1] & 0x3F) << 6 | ((unsigned char)InText[i+2] & 0x3F);  i += 3; }
        else               { CP = 0x3F; i += 4; }

        if (CP == L' ') { PenX += GlyphWidth; continue; }
        
        char buf[128];
        sprintf_s(buf, "CP: %u (0x%X) → %s\n", CP, CP, GetCharInfo(static_cast<wchar_t>(CP)) ? "찾음" : "없음");
        OutputDebugStringA(buf);
        const CharacterInfo* Info = GetCharInfo(static_cast<wchar_t>(CP));
        if (!Info) { PenX += GlyphWidth; continue; }

        const float x0 = PenX,          x1 = PenX + GlyphWidth;
        const float y0 = -GlyphHeight * 0.5f, y1 = GlyphHeight * 0.5f;
        const float u0 = Info->u,              v0 = Info->v;
        const float u1 = Info->u + Info->width, v1 = Info->v + Info->height;

        Vertices.push_back(FTextVertex(FVector(x0, y0, 0.f), u0, v0));
        Vertices.push_back(FTextVertex(FVector(x1, y0, 0.f), u1, v0));
        Vertices.push_back(FTextVertex(FVector(x0, y1, 0.f), u0, v1));
        Vertices.push_back(FTextVertex(FVector(x1, y0, 0.f), u1, v0));
        Vertices.push_back(FTextVertex(FVector(x1, y1, 0.f), u1, v1));
        Vertices.push_back(FTextVertex(FVector(x0, y1, 0.f), u0, v1));

        PenX += GlyphWidth;
    }

    return Vertices;
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