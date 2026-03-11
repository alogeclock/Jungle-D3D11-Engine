#pragma once

#include <string>
#include <vector>
#include "Gameplay/Item.h"
#include "Gameplay/Monster.h"

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

    int  GetMonsterCount() const;
    void SetMonsterCount(int InMonsterCount);

    void AddMonsterType(EMonsterType InType);
    const std::vector<EMonsterType> &GetMonsterTypes() const;
    void ClearMonsterTypes();

    void               SetStageName(const std::string &InStageName);
    const std::string &GetStageName() const;

    void        SetSpawnPoint(int InSpawnX, int InSpawnY);
    FSpawnPoint GetSpawnPoint() const;

    void SetBpm(int InBpm);
    int  GetBpm() const;

    void               SetMusicPath(const std::string &InPath);
    const std::string &GetMusicPath() const;

    void                            AddIntroMessage(const std::string &InMessage);
    void                            SetIntroMessages(const std::vector<std::string> &InMessages);
    const std::vector<std::string> &GetIntroMessages() const;
    void                            ClearIntroMessages();

    void  SetTimeLimit(float InTimeLimit);
    float GetTimeLimit() const;

    void  SetAngryTime(float InAngryTime);
    float GetAngryTime() const;

    void  SetAngryTimeScale(float InScale);
    float GetAngryTimeScale() const;

    void                          AddItem(const FItemData &Item);
    const std::vector<FItemData> &GetItems() const;
    void                          ClearItems();

    void ResizeRenderLayers(int InWidth, int InHeight);
    void SetRenderLayer(int X, int Y, int Value);
    int  GetRenderLayer(int X, int Y) const;

  private:
    std::vector<std::vector<int>> Tiles;
    std::vector<std::vector<int>> RenderLayers;

    int         Width = 0;
    int         Height = 0;
    int         StageId = 0;
    std::string StageName;
    FSpawnPoint PlayerSpawn;
    int         Bpm = 120;
    std::string MusicPath;
    float       TimeLimit = 60.0f;
    float       AngryTime = 0.0f;       // 남은 시간이 이 값 이하일 때 Angry 모드 발동 (0 = 없음)
    float       AngryTimeScale = 1.3f;  // Angry 모드 시 BeatSystem TimeScale
    int         MonsterCount = 0;

    std::vector<std::string> IntroMessages;
    std::vector<EMonsterType> MonsterTypes;
    std::vector<FItemData>    Items;
};
