#include "Stage.h"
#include "MapData.h"
#include "Player.h"
#include "Monster.h"
#include "BeatSystem.h"
#include "Camera2D.h"
#include "ScoreSystem.h"
#include "../Core/GameContext.h"

bool FStage::Load(const std::string& MapPath)
{
	Map = std::make_unique<FMapData>();
	Player = std::make_unique<FPlayer>();
	BeatSystem = std::make_unique<FBeatSystem>();
	Camera = std::make_unique<FCamera2D>();
	ScoreSystem = std::make_unique<FScoreSystem>();

	return true;
}

void FStage::Reset()
{
	Monsters.clear();
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
