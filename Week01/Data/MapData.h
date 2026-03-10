#pragma once

#include <string>
#include <vector>

class FMapData
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

    void SetSpawnPoint(int InSpawnX, int InSpawnY);
    int  GetSpawnX() const;
    int  GetSpawnY() const;

  private:
    // 해석은 TileType enum값 대로
    std::vector<std::vector<int>> Tiles;

    // metadata
    int         Width = 0;
    int         Height = 0;
    int         StageId = 0;
    std::string StageName;
    int         SpawnX = 0;
    int         SpawnY = 0;
};
