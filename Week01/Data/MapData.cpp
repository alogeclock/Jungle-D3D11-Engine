#include "MapData.h"

void FMapData::Clear()
{
    Tiles.clear();
    Width = 0;
    Height = 0;
    StageId = 0;
    StageName.clear();
    SpawnX = 0;
    SpawnY = 0;
}

void FMapData::Resize(int InWidth, int InHeight)
{
    if (InWidth <= 0 || InHeight <= 0)
    {
        Clear();
        return;
    }

    Width = InWidth;
    Height = InHeight;
    Tiles.assign(Height, std::vector<int>(Width, 0));
}

void FMapData::SetTile(int X, int Y, int Value)
{
    if (!IsInside(X, Y))
    {
        return;
    }

    Tiles[Y][X] = Value;
}

int FMapData::GetTile(int X, int Y) const
{
    if (!IsInside(X, Y))
    {
        return 2;
    }

    return Tiles[Y][X];
}

bool FMapData::IsInside(int X, int Y) const { return X >= 0 && X < Width && Y >= 0 && Y < Height; }

bool FMapData::IsWalkable(int X, int Y) const
{
    const int Tile = GetTile(X, Y);
    return Tile == 0 || Tile == 3;
}

int FMapData::GetWidth() const { return Width; }

int FMapData::GetHeight() const { return Height; }

float FMapData::GetWorldWidth(float TileSize) const { return static_cast<float>(Width) * TileSize; }

float FMapData::GetWorldHeight(float TileSize) const
{
    return static_cast<float>(Height) * TileSize;
}

void FMapData::SetStageId(int InStageId) { StageId = InStageId; }

int FMapData::GetStageId() const { return StageId; }

void FMapData::SetStageName(const std::string &InStageName) { StageName = InStageName; }

const std::string &FMapData::GetStageName() const { return StageName; }

void FMapData::SetSpawnPoint(int InSpawnX, int InSpawnY)
{
    SpawnX = InSpawnX;
    SpawnY = InSpawnY;
}

int FMapData::GetSpawnX() const { return SpawnX; }

int FMapData::GetSpawnY() const { return SpawnY; }
