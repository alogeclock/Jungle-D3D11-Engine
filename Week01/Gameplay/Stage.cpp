#include "Stage.h"
#include "MapData.h"
#include "Player.h"
#include "Monster.h"
#include "BeatSystem.h"
#include "Camera2D.h"
#include "ScoreSystem.h"
#include "../Core/GameContext.h"
#include "../Data/MapLoader.h"

bool FStage::Load(const std::string& MapPath)
{
	Map = std::make_unique<FMapData>();
	Player = std::make_unique<FPlayer>();
	BeatSystem = std::make_unique<FBeatSystem>();
	Camera = std::make_unique<FCamera2D>();
	ScoreSystem = std::make_unique<FScoreSystem>();

	// MapLoader로 맵 파일 로드
	FMapLoader Loader;
	if (!Loader.LoadFromFile(MapPath, *Map))
	{
		return false;
	}

	// 타일값 기반으로 오브젝트 배치
	// 0=바닥, 1=벽, 2=플레이어 스폰, 3=몬스터 스폰
	Monsters.clear();
	Tiles.clear();
	Walls.clear();

	for (int Y = 0; Y < Map->GetHeight(); Y++)
	{
		for (int X = 0; X < Map->GetWidth(); X++)
		{
			int Tile = Map->GetTile(X, Y);

			if (Tile == 0)
			{
				Tiles.emplace_back(X, Y, ETileType::Floor);
			}
			else if (Tile == 1)
			{
				Walls.emplace_back(X, Y, EWallType::Normal);
			}
			else if (Tile == 2)
			{
				Tiles.emplace_back(X, Y, ETileType::Floor);
				Player->SetPosition(X, Y, TileSize);
				Map->SetTile(X, Y, 0);
			}
			else if (Tile == 3)
			{
				Tiles.emplace_back(X, Y, ETileType::Floor);
				auto Monster = std::make_unique<FMonster>();
				Monster->SetPosition(X, Y, TileSize);
				Monsters.push_back(std::move(Monster));
				Map->SetTile(X, Y, 0);
			}
		}
	}

	// 카메라 월드 범위 설정
	Camera->SetWorldBounds(
		Map->GetWorldWidth(TileSize),
		Map->GetWorldHeight(TileSize)
	);

	// 비트 시스템 초기화
	BeatSystem->Reset();
	ScoreSystem->Reset();

	bIsGameOver = false;
	bIsCleared = false;

	return true;
}

void FStage::Reset()
{
	Monsters.clear();
	Tiles.clear();
	Walls.clear();
	if (BeatSystem) BeatSystem->Reset();
	if (Camera) Camera->Reset();
	if (ScoreSystem) ScoreSystem->Reset();
	bIsGameOver = false;
	bIsCleared = false;
}

void FStage::Update(FGameContext& Context)
{
}

void FStage::Render(FGameContext& Context)
{
}

bool FStage::IsWalkable(int X, int Y) const
{
	if (!Map) return false;
	return Map->IsWalkable(X, Y);
}

bool FStage::IsOccupied(int X, int Y) const
{
	return FindActorAt(X, Y) != nullptr;
}

bool FStage::CanMoveTo(int X, int Y) const
{
	return IsWalkable(X, Y) && !IsOccupied(X, Y);
}

FActor* FStage::FindActorAt(int X, int Y)
{
	if (Player && Player->GetTileX() == X && Player->GetTileY() == Y)
	{
		return Player.get();
	}

	for (auto& Mon : Monsters)
	{
		if (Mon->GetTileX() == X && Mon->GetTileY() == Y)
		{
			return Mon.get();
		}
	}

	return nullptr;
}

const FActor* FStage::FindActorAt(int X, int Y) const
{
	if (Player && Player->GetTileX() == X && Player->GetTileY() == Y)
	{
		return Player.get();
	}

	for (const auto& Mon : Monsters)
	{
		if (Mon->GetTileX() == X && Mon->GetTileY() == Y)
		{
			return Mon.get();
		}
	}

	return nullptr;
}

FPlayer& FStage::GetPlayer()
{
	return *Player;
}

const FPlayer& FStage::GetPlayer() const
{
	return *Player;
}

void FStage::AddMonster(std::unique_ptr<FMonster> Monster)
{
	Monsters.push_back(std::move(Monster));
}

std::vector<std::unique_ptr<FMonster>>& FStage::GetMonsters()
{
	return Monsters;
}

const std::vector<std::unique_ptr<FMonster>>& FStage::GetMonsters() const
{
	return Monsters;
}

std::vector<FTile>& FStage::GetTiles()
{
	return Tiles;
}

const std::vector<FTile>& FStage::GetTiles() const
{
	return Tiles;
}

std::vector<FWall>& FStage::GetWalls()
{
	return Walls;
}

const std::vector<FWall>& FStage::GetWalls() const
{
	return Walls;
}

FWall* FStage::FindWallAt(int X, int Y)
{
	for (auto& Wall : Walls)
	{
		if (!Wall.IsDestroyed() && Wall.GetTileX() == X && Wall.GetTileY() == Y)
		{
			return &Wall;
		}
	}
	return nullptr;
}

void FStage::RemoveDestroyedWalls()
{
	for (auto It = Walls.begin(); It != Walls.end();)
	{
		if (It->IsDestroyed())
		{
			// MapData도 바닥으로 변경
			if (Map)
			{
				Map->SetTile(It->GetTileX(), It->GetTileY(), 0);
			}
			It = Walls.erase(It);
		}
		else
		{
			++It;
		}
	}
}

FMapData& FStage::GetMap()
{
	return *Map;
}

const FMapData& FStage::GetMap() const
{
	return *Map;
}

FBeatSystem& FStage::GetBeatSystem()
{
	return *BeatSystem;
}

const FBeatSystem& FStage::GetBeatSystem() const
{
	return *BeatSystem;
}

FCamera2D& FStage::GetCamera()
{
	return *Camera;
}

const FCamera2D& FStage::GetCamera() const
{
	return *Camera;
}

FScoreSystem& FStage::GetScoreSystem()
{
	return *ScoreSystem;
}

const FScoreSystem& FStage::GetScoreSystem() const
{
	return *ScoreSystem;
}

float FStage::GetTileSize() const
{
	return TileSize;
}

bool FStage::IsGameOver() const
{
	return bIsGameOver;
}

bool FStage::IsCleared() const
{
	return bIsCleared;
}
