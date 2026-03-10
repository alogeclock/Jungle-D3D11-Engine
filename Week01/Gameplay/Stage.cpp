#include "pch.h"
#include "Stage.h"
#include "BeatSystem.h"
#include "Camera2D.h"
#include "Core/GameContext.h"
#include "Core/Input.h"
#include "Core/AudioSystem.h"
#include "Core/Logger.h"
#include "Data/StageData.h"
#include "Data/StageLoader.h"
#include "IO/ImageLoader.h"
#include "Monster.h"
#include "Player.h"
#include "Render/Renderer.h"
#include "Render/Texture.h"
#include "Render/TextureManager.h"
#include "ScoreSystem.h"
#include "SpriteInfo.h"

FStage::~FStage() = default;

bool FStage::Load(int StageIndex, FRenderer* InRenderer, FTextureManager* InTextures)
{
	Renderer = InRenderer;
	Textures = InTextures;

	Map = std::make_unique<FStageData>();
	Player = std::make_unique<FPlayer>();
	BeatSystem = std::make_unique<FBeatSystem>();
	Camera = std::make_unique<FCamera2D>();
	ScoreSystem = std::make_unique<FScoreSystem>();

	// 싱글턴 StageLoader에서 스테이지 로드
	if (!FStageLoader::Get().LoadStageById(StageIndex, *Map))
	{
		return false;
	}

	CurrentStageIndex = StageIndex;
	StageName = Map->GetStageName();

	// 타일값 기반으로 오브젝트 배치
	// 0=PATH, 1=WALL, 2=OUTER, 3=GOAL
	Monsters.clear();
	Tiles.clear();
	Walls.clear();
	GoalX = -1;
	GoalY = -1;

	for (int Y = 0; Y < Map->GetHeight(); Y++)
	{
		for (int X = 0; X < Map->GetWidth(); X++)
		{
			int        TileVal = Map->GetTile(X, Y);
			ETileValue TV = static_cast<ETileValue>(TileVal);

			switch (TV)
			{
			case ETileValue::Path:
				Tiles.emplace_back(X, Y, ETileType::Floor);
				break;
			case ETileValue::Wall:
				Walls.emplace_back(X, Y, EWallType::Normal);
				break;
			case ETileValue::Outer:
				break;
			case ETileValue::Goal:
				Tiles.emplace_back(X, Y, ETileType::Goal);
				GoalX = X;
				GoalY = Y;
				break;
			}
		}
	}

	// 메타데이터 기반 플레이어 스폰
	FSpawnPoint Spawn = Map->GetSpawnPoint();
	Player->SetPosition(Spawn.X, Spawn.Y, TileSize);

	// 카메라 설정
	Camera->SetWorldBounds(Map->GetWorldWidth(TileSize), Map->GetWorldHeight(TileSize));
	Camera->SetViewportSize(Renderer->ViewportInfo.Width, Renderer->ViewportInfo.Height);

	// 스프라이트 리소스 로드 + 엔티티에 스프라이트 할당
	LoadSpriteResources();

	// 비트 시스템 초기화 (스테이지 BPM 적용)
	BeatSystem->SetBpm(static_cast<float>(Map->GetBpm()));
	BeatSystem->Reset();
	ScoreSystem->Reset();

	// BGM 재생
	FAudioSystem::Get().StopAll();
	const std::string& MusicPath = Map->GetMusicPath();
	if (!MusicPath.empty())
	{
		std::string BgmKey = "bgm_stage" + std::to_string(StageIndex);
		FAudioSystem::Get().LoadWav(BgmKey, MusicPath);
		FAudioSystem::Get().Play(BgmKey, true);
	}

	bIsGameOver = false;
	bIsCleared = false;

	return true;
}

void FStage::Reset()
{
	Monsters.clear();
	Tiles.clear();
	Walls.clear();
	if (BeatSystem)
		BeatSystem->Reset();
	if (Camera)
		Camera->Reset();
	if (ScoreSystem)
		ScoreSystem->Reset();
	bIsGameOver = false;
	bIsCleared = false;
}

void FStage::Update(float DeltaTime, FGameContext& Context)
{
	// 비트 시스템 업데이트
	BeatSystem->Update(DeltaTime, Context);

	// 액터 이동 업데이트
	Player->Update(DeltaTime, Context);
	for (auto& Mon : Monsters)
	{
		Mon->Update(DeltaTime, Context);
	}

	EDirection MoveDir;
	bool       bHasInput = false;

	if (Context.Input.GetKeyDown(EKeyCode::Up))
	{
		MoveDir = EDirection::Up;
		bHasInput = true;
	}
	else if (Context.Input.GetKeyDown(EKeyCode::Down))
	{
		MoveDir = EDirection::Down;
		bHasInput = true;
	}
	else if (Context.Input.GetKeyDown(EKeyCode::Left))
	{
		MoveDir = EDirection::Left;
		bHasInput = true;
		FSpriteInfo Spr = Player->GetSprite();
		Spr.bIsMirrored = false;
		Player->SetSprite(Spr);
	}
	else if (Context.Input.GetKeyDown(EKeyCode::Right))
	{
		MoveDir = EDirection::Right;
		bHasInput = true;
		FSpriteInfo Spr = Player->GetSprite();
		Spr.bIsMirrored = true;
		Player->SetSprite(Spr);
	}

	if (!Player->IsDead() && bHasInput)
	{
		// 입력이 들어온 현재 시점의 박자 인덱스
		int CurrentBeatIndex =
			static_cast<int>(BeatSystem->GetElapsedTime() / BeatSystem->GetBeatInterval());

		if (BeatSystem->JudgeInput() == EBeatJudge::Good)
		{
			Logger::Log("Good Input");
			if (Player->GetLastMovedBeatIndex() == CurrentBeatIndex)
			{
				Player->Damage(1); // 한 박자 내 중복 이동 시 데미지
			}
			else
			{
				// 즉시 이동 처리
				Player->QueueInput(MoveDir);
				Player->OnBeat(*this);
				Player->SetLastMovedBeatIndex(CurrentBeatIndex);
			}
		}
		else
		{
			Logger::Log("Miss Input");
			Player->Damage(1); // 엇박자 입력 시 데미지
			Player->SetLastMovedBeatIndex(CurrentBeatIndex);
		}
	}

	// 3. 비트 시작 시 처리 (박자가 넘어가는 순간에만 1회 수행)
	if (BeatSystem->ConsumeBeat()) //
	{
		int CurrentBeatIndex =
			static_cast<int>(BeatSystem->GetElapsedTime() / BeatSystem->GetBeatInterval());

		// 가만히 있으면 데미지 로직: 이전 박자 동안 이동 기록이 없으면 데미지
		if (!Player->IsDead() && CurrentBeatIndex > 0)
		{
			// bHasInput은 현재 프레임의 상태일 뿐, "이전 박자 동안 움직였는가"는 Index로만
			// 판단합니다.
			if (Player->GetLastMovedBeatIndex() < (CurrentBeatIndex - 1))
			{
				Logger::Log("No Input Detected - Player Damaged");
				Player->Damage(1);
			}
		}

		// 몬스터 이동
		for (auto& Mon : Monsters)
		{
			Mon->OnBeat(*this);
		}
	}

	// 플레이어 사망 체크
	if (Player->IsDead())
	{
		bIsGameOver = true;
	}

	// 골인 지점 도달 체크
	if (GoalX >= 0 && Player->GetTileX() == GoalX && Player->GetTileY() == GoalY)
	{
		bIsCleared = true;
	}

	// 카메라가 플레이어를 추적
	FVec2 PlayerCenter;
	PlayerCenter.X = Player->GetRenderX() + TileSize * 0.5f;
	PlayerCenter.Y = Player->GetRenderY() + TileSize * 0.5f;
	Camera->SetTargetCenter(PlayerCenter);
	Camera->Update(DeltaTime);
}

void FStage::Render()
{
	if (!Renderer || !Textures)
		return;

	// Renderer에 카메라 설정
	Renderer->SetCamera(*Camera);

	// 텍스처 룩업 헬퍼
	auto GetTex = [&](const std::string& Key) -> FTexture*
		{
			return (!Key.empty() && Textures) ? Textures->Get(Key) : nullptr;
		};

	// 바닥 타일 렌더링
	for (const auto& Tile : Tiles)
	{
		float WorldX = Tile.GetRenderX(TileSize) + TileSize * 0.5f;
		float WorldY = Tile.GetRenderY(TileSize) + TileSize * 0.5f;
		const FSpriteInfo& Spr = Tile.GetSprite();
		Renderer->DrawSprite(GetTex(Spr.TextureKey), WorldX, WorldY, TileSize, TileSize, Spr);
	}

	// 벽 렌더링
	for (const auto& W : Walls)
	{
		if (W.IsDestroyed())
			continue;
		float WorldX = W.GetRenderX(TileSize) + TileSize * 0.5f;
		float WorldY = W.GetRenderY(TileSize) + TileSize * 0.5f;
		const FSpriteInfo& Spr = W.GetSprite();
		Renderer->DrawSprite(GetTex(Spr.TextureKey), WorldX, WorldY, TileSize, TileSize, Spr);
	}

	// 몬스터 렌더링
	for (const auto& Mon : Monsters)
	{
		if (Mon->IsDead())
			continue;
		float WorldX = Mon->GetRenderX() + TileSize * 0.5f;
		float WorldY = Mon->GetRenderY() + TileSize * 0.5f;
		const FSpriteInfo& Spr = Mon->GetSprite();
		Renderer->DrawSprite(GetTex(Spr.TextureKey), WorldX, WorldY, TileSize, TileSize, Spr);
	}

	// 플레이어 렌더링 (가장 위에 그림)
	if (!Player->IsDead())
	{
		float WorldX = Player->GetRenderX() + TileSize * 0.5f;
		float WorldY = Player->GetRenderY() + TileSize * 0.5f;
		const FSpriteInfo& Spr = Player->GetSprite();
		Renderer->DrawSprite(GetTex(Spr.TextureKey), WorldX, WorldY, TileSize, TileSize, Spr);
	}
}

// ============================================================
// 스프라이트 리소스 관리
// ============================================================

void FStage::LoadSpriteResources()
{
	if (!Textures)
		return;

	// 스프라이트 텍스처 로드 (파일이 없으면 셰이더 폴백 색상 사용)
	auto LoadTex = [&](const std::string& Key, const std::string& Path)
		{
			if (!Textures->Has(Key))
			{
				auto Tex = FImageLoader::LoadAsTexture(Renderer->Device, Path);
				if (Tex)
					Textures->Register(Key, std::move(Tex));
			}
		};
	LoadTex("tile_floor", "Resources/Sprites/tile_floor.png");
	LoadTex("goal", "Resources/Sprites/goal.png");
	LoadTex("wall", "Resources/Sprites/wall.png");
	LoadTex("player", "Resources/Sprites/player.png");
	LoadTex("monster", "Resources/Sprites/monster.png");

	// 타일에 스프라이트 할당
	for (auto& Tile : Tiles)
	{
		FSpriteInfo Info;
		Info.TextureKey = (Tile.GetType() == ETileType::Goal) ? "goal" : "tile_floor";
		Info.SpriteSize = { TileSize, TileSize };
		Tile.SetSprite(Info);
	}

	// 벽에 스프라이트 할당
	for (auto& W : Walls)
	{
		FSpriteInfo Info;
		Info.TextureKey = "wall";
		Info.SpriteSize = { TileSize, TileSize };
		W.SetSprite(Info);
	}

	// 플레이어 스프라이트
	{
		FSpriteInfo Info;
		Info.TextureKey = "player";
		Info.SpriteSize = { TileSize, TileSize };
		Player->SetSprite(Info);
	}

	// 몬스터 스프라이트
	for (auto& Mon : Monsters)
	{
		FSpriteInfo Info;
		Info.TextureKey = "monster";
		Info.SpriteSize = { TileSize, TileSize };
		Mon->SetSprite(Info);
	}
}

bool FStage::IsWalkable(int X, int Y) const
{
	if (!Map)
		return false;
	return Map->IsWalkable(X, Y);
}

bool FStage::IsOccupied(int X, int Y) const { return FindActorAt(X, Y) != nullptr; }

bool FStage::CanMoveTo(int X, int Y) const { return IsWalkable(X, Y); }

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

FPlayer& FStage::GetPlayer() { return *Player; }

const FPlayer& FStage::GetPlayer() const { return *Player; }

void FStage::AddMonster(std::unique_ptr<FMonster> Monster)
{
	Monsters.push_back(std::move(Monster));
}

std::vector<std::unique_ptr<FMonster>>& FStage::GetMonsters() { return Monsters; }

const std::vector<std::unique_ptr<FMonster>>& FStage::GetMonsters() const { return Monsters; }

std::vector<FTile>& FStage::GetTiles() { return Tiles; }

const std::vector<FTile>& FStage::GetTiles() const { return Tiles; }

std::vector<FWall>& FStage::GetWalls() { return Walls; }

const std::vector<FWall>& FStage::GetWalls() const { return Walls; }

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

FStageData& FStage::GetMap() { return *Map; }

const FStageData& FStage::GetMap() const { return *Map; }

FBeatSystem& FStage::GetBeatSystem() { return *BeatSystem; }

const FBeatSystem& FStage::GetBeatSystem() const { return *BeatSystem; }

FCamera2D& FStage::GetCamera() { return *Camera; }

const FCamera2D& FStage::GetCamera() const { return *Camera; }

FScoreSystem& FStage::GetScoreSystem() { return *ScoreSystem; }

const FScoreSystem& FStage::GetScoreSystem() const { return *ScoreSystem; }

float FStage::GetTileSize() const { return TileSize; }

bool FStage::IsGameOver() const { return bIsGameOver; }

bool FStage::IsCleared() const { return bIsCleared; }

int FStage::GetCurrentStageIndex() const { return CurrentStageIndex; }

const std::string& FStage::GetStageName() const { return StageName; }
