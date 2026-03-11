#include "pch.h"
#include "Core/Types.h"
#include "StageData.h"

void FStageData::Clear()
{
    Tiles.clear();
    Width = 0;
    Height = 0;
    StageId = 0;
    StageName.clear();
    PlayerSpawn = {};
    Bpm = 120;
    MusicPath.clear();
    TimeLimit = 60.0f;
}

void FStageData::Resize(int InWidth, int InHeight)
{
    if (InWidth < 0)
    {
        InWidth = 0;
    }

    if (InHeight < 0)
    {
        InHeight = 0;
    }

    Width = InWidth;
    Height = InHeight;
    Tiles.assign(Height, std::vector<int>(Width, 0));
}

void FStageData::SetTile(int X, int Y, int Value)
{
    if (IsInside(X, Y))
    {
        Tiles[Y][X] = Value;
    }
}

int FStageData::GetTile(int X, int Y) const
{
    if (IsInside(X, Y))
    {
        return Tiles[Y][X];
    }

    return -1;
}

bool FStageData::IsInside(int X, int Y) const
{
    return X >= 0 && X < Width && Y >= 0 && Y < Height;
}

bool FStageData::IsWalkable(int X, int Y) const
{
    if (!IsInside(X, Y))
    {
        return false;
    }

    const int TileValue = Tiles[Y][X];

    return TileValue == static_cast<int>(ETileValue::Path) ||
           TileValue == static_cast<int>(ETileValue::Goal);
}

int FStageData::GetWidth() const { return Width; }

int FStageData::GetHeight() const { return Height; }

float FStageData::GetWorldWidth(float TileSize) const
{
    return static_cast<float>(Width) * TileSize;
}

float FStageData::GetWorldHeight(float TileSize) const
{
    return static_cast<float>(Height) * TileSize;
}

void FStageData::SetStageId(int InStageId) { StageId = InStageId; }

int FStageData::GetStageId() const { return StageId; }

void FStageData::SetStageName(const std::string &InStageName) { StageName = InStageName; }

int FStageData::GetMonsterCount() const { return MonsterCount; }

void FStageData::SetMonsterCount(int InMonsterCount) { MonsterCount = InMonsterCount; }

const std::string &FStageData::GetStageName() const { return StageName; }

void FStageData::SetSpawnPoint(int InSpawnX, int InSpawnY)
{
    PlayerSpawn.X = InSpawnX;
    PlayerSpawn.Y = InSpawnY;
}

FSpawnPoint FStageData::GetSpawnPoint() const { return PlayerSpawn; }

void FStageData::SetBpm(int InBpm) { Bpm = InBpm; }

int FStageData::GetBpm() const { return Bpm; }

void FStageData::SetMusicPath(const std::string &InPath) { MusicPath = InPath; }

const std::string &FStageData::GetMusicPath() const { return MusicPath; }

void FStageData::SetTimeLimit(float InTimeLimit) { TimeLimit = InTimeLimit; }

float FStageData::GetTimeLimit() const { return TimeLimit; }
