#pragma once

#include <string>
#include <vector>
#include "Gameplay/Item.h"

struct FSpawnPoint
{
    int X = 0;
    int Y = 0;
};

struct FStageData
{
  public:
    void Clear();
    void Resize(int InWidth, int InHeight);

    void SetTile(int X, int Y, int Value);
    int  GetTile(int X, int Y) const;

    bool IsInside(int X, int Y) const;
    bool IsWalkable(int X, int Y) const;

    int GetWidth() const;
    int GetHeight() const;

    float GetWorldWidth(float TileSize) const;
    float GetWorldHeight(float TileSize) const;

    void SetStageId(int InStageId);
    int  GetStageId() const;

    void               SetStageName(const std::string &InStageName);
    const std::string &GetStageName() const;

    void        SetSpawnPoint(int InSpawnX, int InSpawnY);
    FSpawnPoint GetSpawnPoint() const;

    void SetBpm(int InBpm);
    int  GetBpm() const;

    void               SetMusicPath(const std::string &InPath);
    const std::string &GetMusicPath() const;

    void  SetTimeLimit(float InTimeLimit);
    float GetTimeLimit() const;

    void                          AddItem(const FItemData &Item);
    const std::vector<FItemData> &GetItems() const;
    void                          ClearItems();

  private:
    // 해석은 TileType enum값 대로
    std::vector<std::vector<int>> Tiles;

    // metadata
    int         Width = 0;
    int         Height = 0;
    int         StageId = 0;
    std::string StageName;
    FSpawnPoint PlayerSpawn;
    int         Bpm = 120;
    std::string MusicPath;
    float       TimeLimit = 60.0f;

    std::vector<FItemData> Items;
};
