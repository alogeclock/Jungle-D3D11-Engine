#pragma once
#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>
#include "BeatSystem.h"
#include "Camera2D.h"
#include "Data/StageData.h"
#include "Monster.h"
#include "Player.h"
#include "ScoreSystem.h"
#include "Tile.h"
#include "Wall.h"
#include <memory>
#include <string>
#include <vector>

class FActor;
class FRenderer;
class FTextureManager;

class FStage
{
  public:
    ~FStage();

    bool Load(int StageIndex, FRenderer *InRenderer, FTextureManager *InTextures);
    void Reset();

    void Update(float DeltaTime, FGameContext &Context);
    void Render();

    bool IsWalkable(int X, int Y) const;
    bool IsOccupied(int X, int Y) const;
    bool CanMoveTo(int X, int Y) const;

    FActor       *FindActorAt(int X, int Y);
    const FActor *FindActorAt(int X, int Y) const;

    FPlayer       &GetPlayer();
    const FPlayer &GetPlayer() const;

    void                                          AddMonster(std::unique_ptr<FMonster> Monster);
    std::vector<std::unique_ptr<FMonster>>       &GetMonsters();
    const std::vector<std::unique_ptr<FMonster>> &GetMonsters() const;

    std::vector<FTile>       &GetTiles();
    const std::vector<FTile> &GetTiles() const;

    std::vector<FWall>       &GetWalls();
    const std::vector<FWall> &GetWalls() const;
    FWall                    *FindWallAt(int X, int Y);
    void                      RemoveDestroyedWalls();

    FStageData       &GetMap();
    const FStageData &GetMap() const;

    FBeatSystem       &GetBeatSystem();
    const FBeatSystem &GetBeatSystem() const;

    FCamera2D       &GetCamera();
    const FCamera2D &GetCamera() const;

    FScoreSystem       &GetScoreSystem();
    const FScoreSystem &GetScoreSystem() const;
    int                 GetScore() const;

    float GetTileSize() const;

    bool IsGameOver() const;
    bool IsCleared() const;

    int                GetCurrentStageIndex() const;
    const std::string &GetStageName() const;

  private:
    std::unique_ptr<FStageData>            Map;
    std::unique_ptr<FPlayer>               Player;
    std::vector<std::unique_ptr<FMonster>> Monsters;
    std::vector<FTile>                     Tiles;
    std::vector<FWall>                     Walls;

    std::unique_ptr<FBeatSystem>  BeatSystem;
    std::unique_ptr<FCamera2D>    Camera;
    std::unique_ptr<FScoreSystem> ScoreSystem;

    float TileSize = 96.0f;

    bool bIsGameOver = false;
    bool bIsCleared = false;

    int         GoalX = -1;
    int         GoalY = -1;
    int         CurrentStageIndex = 0;
    std::string StageName;

    FRenderer       *Renderer = nullptr;
    FTextureManager *Textures = nullptr;

	void LoadSpriteResources();
};
