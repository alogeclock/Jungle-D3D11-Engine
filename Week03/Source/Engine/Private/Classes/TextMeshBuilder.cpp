#include "Source/Engine/Public/Classes/TextMeshBuilder.h"

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
}

const CharacterInfo *FTextMeshBuilder::GetCharInfo(wchar_t InChar)
{
    auto It = charInfoMap.find(InChar);
    if (It != charInfoMap.end()) return &It->second;
    return nullptr;
}

TArray<FTextVertex> FTextMeshBuilder::BuildTextMesh(const FString &InText)
{
    //현재는 중심 기준
    if (charInfoMap.empty())
        InitializeCharInfo();

    TArray<FTextVertex> Vertices;
    Vertices.reserve(InText.size() * 6);

    const float StartX = -(static_cast<float>(InText.size()) * 0.5f);
    float PenX = StartX / 2;

    for (char Ch8 : InText)
    {
        const wchar_t Ch = static_cast<unsigned char>(Ch8);
        const CharacterInfo* Info = GetCharInfo(Ch);
        if (!Info)
        {
            PenX += 0.5f;
            continue;
        }

        const float x0 = PenX;
        const float x1 = PenX + 0.5f;
        const float y0 = -0.25f;
        const float y1 = 0.25f;
        const float z  = 0.0f;

        const float u0 = Info->u;
        const float v0 = Info->v;
        const float u1 = Info->u + Info->width;
        const float v1 = Info->v + Info->height;

        Vertices.push_back({ FVector<float>(x0, y0, 0.0f), u0, v0 });
        Vertices.push_back({ FVector<float>(x1, y0, 0.0f), u1, v0 });
        Vertices.push_back({ FVector<float>(x0, y1, 0.0f), u0, v1 });

        Vertices.push_back({ FVector<float>(x1, y0, 0.0f), u1, v0 });
        Vertices.push_back({ FVector<float>(x1, y1, 0.0f), u1, v1 });
        Vertices.push_back({ FVector<float>(x0, y1, 0.0f), u0, v1 });

        PenX += 0.5f;
    }

    return Vertices;
}

void FTextMeshBuilder::LoadFNT(const std::string& FntPath)
{
    
}
