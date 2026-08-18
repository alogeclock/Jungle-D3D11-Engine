#include "pch.h"
#include "StageData.h"

void FStageData::Clear()
{
    Tiles.clear();
    RenderLayers.clear();

    Width = 0;
    Height = 0;
    StageId = 0;
    StageName.clear();

    PlayerSpawn = {};
    Bpm = 120;
    MusicPath.clear();

    TimeLimit = 60.0f;
    AngryTime = 0.0f;
    AngryTimeScale = 1.3f;
    MonsterCount = 0;
    DarknessLevel = 2;

    IntroMessages.clear();
    MonsterTypes.clear();
    Items.clear();
}

void FStageData::Resize(int InWidth, int InHeight)
{
    Width = InWidth;
    Height = InHeight;
    Tiles.assign(Height, std::vector<int>(Width, 0));
}

void FStageData::SetTile(int X, int Y, int Value)
{
    if (!IsInside(X, Y))
    {
        return;
    }

    Tiles[Y][X] = Value;
}

int FStageData::GetTile(int X, int Y) const
{
    if (!IsInside(X, Y))
    {
        return -1;
    }

    return Tiles[Y][X];
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

    const int Tile = GetTile(X, Y);
    return Tile == 0 || Tile == 3 || Tile == 4;
}

int FStageData::GetWidth() const
{
    return Width;
}

int FStageData::GetHeight() const
{
    return Height;
}

float FStageData::GetWorldWidth(float TileSize) const
{
    return static_cast<float>(Width) * TileSize;
}

float FStageData::GetWorldHeight(float TileSize) const
{
    return static_cast<float>(Height) * TileSize;
}

void FStageData::SetStageId(int InStageId)
{
    StageId = InStageId;
}

int FStageData::GetStageId() const
{
    return StageId;
}

void FStageData::SetStageName(const std::string &InStageName)
{
    StageName = InStageName;
}

const std::string &FStageData::GetStageName() const
{
    return StageName;
}

void FStageData::SetSpawnPoint(int InSpawnX, int InSpawnY)
{
    PlayerSpawn.X = InSpawnX;
    PlayerSpawn.Y = InSpawnY;
}

FSpawnPoint FStageData::GetSpawnPoint() const
{
    return PlayerSpawn;
}

void FStageData::SetBpm(int InBpm)
{
    Bpm = InBpm;
}

int FStageData::GetBpm() const
{
    return Bpm;
}

void FStageData::SetMusicPath(const std::string &InPath)
{
    MusicPath = InPath;
}

const std::string &FStageData::GetMusicPath() const
{
    return MusicPath;
}

void FStageData::SetTimeLimit(float InTimeLimit)
{
    TimeLimit = InTimeLimit;
}

float FStageData::GetTimeLimit() const
{
    return TimeLimit;
}

void FStageData::SetAngryTime(float InAngryTime)
{
    AngryTime = InAngryTime;
}

float FStageData::GetAngryTime() const
{
    return AngryTime;
}

void FStageData::SetAngryTimeScale(float InScale)
{
    AngryTimeScale = InScale;
}

float FStageData::GetAngryTimeScale() const
{
    return AngryTimeScale;
}

void FStageData::SetMonsterCount(int InMonsterCount)
{
    MonsterCount = InMonsterCount;
}

int FStageData::GetMonsterCount() const
{
    return MonsterCount;
}

void FStageData::AddMonsterType(EMonsterType InType)
{
    MonsterTypes.push_back(InType);
}

const std::vector<EMonsterType> &FStageData::GetMonsterTypes() const
{
    return MonsterTypes;
}

void FStageData::ClearMonsterTypes()
{
    MonsterTypes.clear();
}

void FStageData::AddIntroMessage(const std::string &InMessage)
{
    IntroMessages.push_back(InMessage);
}

void FStageData::SetIntroMessages(const std::vector<std::string> &InMessages)
{
    IntroMessages = InMessages;
}

const std::vector<std::string> &FStageData::GetIntroMessages() const
{
    return IntroMessages;
}

void FStageData::ClearIntroMessages()
{
    IntroMessages.clear();
}

void FStageData::SetDarknessLevel(int InLevel)
{
    DarknessLevel = InLevel;
}

int FStageData::GetDarknessLevel() const
{
    return DarknessLevel;
}

void FStageData::AddItem(const FItemData &Item)
{
    Items.push_back(Item);
}

const std::vector<FItemData> &FStageData::GetItems() const
{
    return Items;
}

void FStageData::ClearItems()
{
    Items.clear();
}

void FStageData::ResizeRenderLayers(int InWidth, int InHeight)
{
    RenderLayers.assign(
        InHeight < 0 ? 0 : InHeight,
        std::vector<int>(InWidth < 0 ? 0 : InWidth, -1));
}

void FStageData::SetRenderLayer(int X, int Y, int Value)
{
    if (IsInside(X, Y) &&
        Y < static_cast<int>(RenderLayers.size()) &&
        X < static_cast<int>(RenderLayers[Y].size()))
    {
        RenderLayers[Y][X] = Value;
    }
}

int FStageData::GetRenderLayer(int X, int Y) const
{
    if (IsInside(X, Y) &&
        Y < static_cast<int>(RenderLayers.size()) &&
        X < static_cast<int>(RenderLayers[Y].size()))
    {
        return RenderLayers[Y][X];
    }

    return -1;
}
