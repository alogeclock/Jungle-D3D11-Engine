#pragma once

#include <memory>
#include <string>
#include <vector>

#include "MapData.h"
#include "Player.h"
#include "Monster.h"
#include "Tile.h"
#include "Wall.h"
#include "BeatSystem.h"
#include "Camera2D.h"
#include "ScoreSystem.h"

class FActor;
struct FGameContext;

class FStage
{
public:
	bool Load(const std::string& MapPath);
	void Reset();

	void Update(FGameContext& Context);
	void Render(FGameContext& Context);

	bool IsWalkable(int X, int Y) const;
	bool IsOccupied(int X, int Y) const;
	bool CanMoveTo(int X, int Y) const;

	FActor* FindActorAt(int X, int Y);
	const FActor* FindActorAt(int X, int Y) const;

	FPlayer& GetPlayer();
	const FPlayer& GetPlayer() const;

	void AddMonster(std::unique_ptr<FMonster> Monster);
	std::vector<std::unique_ptr<FMonster>>& GetMonsters();
	const std::vector<std::unique_ptr<FMonster>>& GetMonsters() const;

	std::vector<FTile>& GetTiles();
	const std::vector<FTile>& GetTiles() const;

	std::vector<FWall>& GetWalls();
	const std::vector<FWall>& GetWalls() const;
	FWall* FindWallAt(int X, int Y);
	void RemoveDestroyedWalls();

	FMapData& GetMap();
	const FMapData& GetMap() const;

	FBeatSystem& GetBeatSystem();
	const FBeatSystem& GetBeatSystem() const;

	FCamera2D& GetCamera();
	const FCamera2D& GetCamera() const;

	FScoreSystem& GetScoreSystem();
	const FScoreSystem& GetScoreSystem() const;

	float GetTileSize() const;

	bool IsGameOver() const;
	bool IsCleared() const;

private:
	std::unique_ptr<FMapData> Map;
	std::unique_ptr<FPlayer> Player;
	std::vector<std::unique_ptr<FMonster>> Monsters;
	std::vector<FTile> Tiles;
	std::vector<FWall> Walls;

	std::unique_ptr<FBeatSystem> BeatSystem;
	std::unique_ptr<FCamera2D> Camera;
	std::unique_ptr<FScoreSystem> ScoreSystem;

	float TileSize = 32.0f;

	bool bIsGameOver = false;
	bool bIsCleared = false;
};
