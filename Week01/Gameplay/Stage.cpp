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
#include <cmath>
#include "ScoreSystem.h"
#include "SpriteInfo.h"

FStage::~FStage() { ReleaseStaticBatches(); }

bool FStage::Load(int StageIndex, FRenderer *InRenderer, FTextureManager *InTextures)
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

    int SpawnMonsterCount = Map->GetMonsterCount();
    int MaxTries = 1000; // 타일 탐색 시 무한 루프 방지용

    for (int i = 0; i < SpawnMonsterCount; ++i)
    {
        int Tries = 0;
        while (Tries < MaxTries)
        {
            // 랜덤한 X, Y 좌표 획득
            int RandX = std::rand() % Map->GetWidth();
            int RandY = std::rand() % Map->GetHeight();

            // 해당 타일이 이동 가능(바닥)하고, 다른 액터(플레이어/다른 몬스터)가 없는지 확인
            if (CanMoveTo(RandX, RandY) && !IsOccupied(RandX, RandY))
            {
                auto NewMonster = std::make_unique<FMonster>();
                NewMonster->SetPosition(RandX, RandY, TileSize);

                // 필요하다면 여기서 AI 종류나 이동 주기를 설정할 수 있습니다.
                NewMonster->SetAiType(EMonsterAIType::ChasePlayer);

                AddMonster(std::move(NewMonster));
                break; // 스폰 성공 시 다음 몬스터 생성으로 넘어감
            }
            Tries++;
        }
    }

    // 카메라 설정
    Camera->SetWorldBounds(Map->GetWorldWidth(TileSize), Map->GetWorldHeight(TileSize));
    Camera->SetViewportSize(Renderer->ViewportInfo.Width, Renderer->ViewportInfo.Height);

    // 스프라이트 리소스 로드 + 엔티티에 스프라이트 할당
    LoadSpriteResources();

    // 정적 배치 생성 (타일/벽을 텍스처별로 묶어 DrawIndexed)
    BuildStaticBatches();

    // 암흑 시야 텍스처 생성
    CreateDarknessTexture();

    // 제한시간 설정
    TimeLimit = Map->GetTimeLimit();
    RemainingTime = TimeLimit;

    // 비트 시스템 초기화 (스테이지 BPM 적용)
    BeatSystem->SetBpm(static_cast<float>(Map->GetBpm()));
    BeatSystem->Reset();
    ScoreSystem->Reset();

    // BGM 재생
    FAudioSystem::Get().StopAll();
    const std::string &MusicPath = Map->GetMusicPath();
    if (!MusicPath.empty())
    {
        std::string BgmKey = "bgm_stage" + std::to_string(StageIndex);
        FAudioSystem::Get().LoadWav(BgmKey, MusicPath);
        FAudioSystem::Get().Play(BgmKey, true);
    }

    // 효과음 로드
    FAudioSystem::Get().LoadWav("sfx_perfect", "Resources/Sounds/perfect.wav");
    FAudioSystem::Get().LoadWav("sfx_good", "Resources/Sounds/good.wav");
    FAudioSystem::Get().LoadWav("sfx_miss", "Resources/Sounds/miss.wav");

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

void FStage::Update(float DeltaTime, FGameContext &Context)
{
    // 제한시간 갱신
    if (!bTimeFrozen)
        RemainingTime -= DeltaTime;
    if (RemainingTime <= 0.0f)
    {
        RemainingTime = 0.0f;
        bIsGameOver = true;
        return;
    }

    // 비트 시스템 업데이트
    BeatSystem->Update(DeltaTime, Context);

    // 액터 이동 업데이트
    Player->Update(DeltaTime, Context);
    for (auto &Mon : Monsters)
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
        EBeatJudge Judge = BeatSystem->JudgeInput();

        if (Judge == EBeatJudge::Perfect)
        {
            Logger::Log("Perfect Input");
            if (Player->GetLastMovedBeatIndex() == CurrentBeatIndex)
            {
                Player->Damage(1); // 한 박자 내 중복 이동 시 데미지
                ScoreSystem->AddBeatBonus(EBeatJudge::Miss);
                FAudioSystem::Get().Play("sfx_miss", false);
            }
            else
            {
                // 즉시 이동 처리
                Player->QueueInput(MoveDir);
                Player->OnBeat(*this);
                ScoreSystem->AddBeatBonus(Judge);
                Player->SetLastMovedBeatIndex(CurrentBeatIndex);
                FAudioSystem::Get().Play("sfx_perfect", false);
            }
        }
        else if (Judge == EBeatJudge::Good)
        {
            Logger::Log("Good Input");
            if (Player->GetLastMovedBeatIndex() == CurrentBeatIndex)
            {
                Player->Damage(1); // 한 박자 내 중복 이동 시 데미지
                ScoreSystem->AddBeatBonus(EBeatJudge::Miss);
                FAudioSystem::Get().Play("sfx_miss", false);
            }
            else
            {
                // 즉시 이동 처리
                Player->QueueInput(MoveDir);
                Player->OnBeat(*this);
                ScoreSystem->AddBeatBonus(Judge);
                Player->SetLastMovedBeatIndex(CurrentBeatIndex);
                FAudioSystem::Get().Play("sfx_good", false);
            }
        }
        else
        {
            Logger::Log("Miss Input");
            Player->Damage(1); // 엇박자 입력 시 데미지
            ScoreSystem->AddBeatBonus(EBeatJudge::Miss);
            Player->SetLastMovedBeatIndex(CurrentBeatIndex);
            FAudioSystem::Get().Play("sfx_miss", false);
        }
    }

    // 3. 비트 시작 시 처리 (박자가 넘어가는 순간에만 1회 수행)
    if (BeatSystem->ConsumeBeat())
    {
        // 몬스터 이동
        for (auto &Mon : Monsters)
        {
            Mon->OnBeat(*this);
        }
    }

    // 비트 스킵 시 미입력 데미지
    if (BeatSystem->IsBeatSkipped())
    {
        int CurrentBeatIndex =
            static_cast<int>(BeatSystem->GetElapsedTime() / BeatSystem->GetBeatInterval());

        if (!Player->IsDead() && CurrentBeatIndex > 0)
        {
            if (Player->GetLastMovedBeatIndex() < (CurrentBeatIndex - 1))
            {
                Logger::Log("No Input Detected - Player Damaged");
                Player->Damage(1);
                FAudioSystem::Get().Play("sfx_miss", false);
            }
        }
    }

    if (!Player->IsDead())
    {
        int px = Player->GetTileX();
        int py = Player->GetTileY();

        for (auto Mon = Monsters.begin(); Mon != Monsters.end();)
        {
            int mx = (*Mon)->GetTileX();
            int my = (*Mon)->GetTileY();

            if (!(*Mon)->IsDead() && mx == px && my == py)
            {
                // 1. 플레이어에게 데미지 1 적용
                Player->Damage(1);

                // 2. 몬스터 소멸 (남은 HP만큼 데미지를 주어 IsDead() 상태로 만듦)
                (*Mon)->Damage((*Mon)->GetHp());
            }

            if ((*Mon)->IsDead())
                Mon = Monsters.erase(Mon);
            else
                ++Mon;
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
        ScoreSystem->AddTimeBonus(RemainingTime, TimeLimit);
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

    // 정적 배치 렌더링 (타일/벽 — DrawIndexed 1회씩)
    Renderer->DrawBatch(FloorBatch, Textures->Get("tile_floor"));
    Renderer->DrawBatch(GoalBatch, Textures->Get("goal"));
    Renderer->DrawBatch(WallBatch, Textures->Get("wall"));

    // 텍스처 룩업 헬퍼
    auto GetTex = [&](const std::string &Key) -> FTexture *
    { return (!Key.empty() && Textures) ? Textures->Get(Key) : nullptr; };

    // 몬스터 렌더링
    for (const auto &Mon : Monsters)
    {
        if (Mon->IsDead())
            continue;
        float              WorldX = Mon->GetRenderX() + TileSize * 0.5f;
        float              WorldY = Mon->GetRenderY() + TileSize * 0.5f;
        const FSpriteInfo &Spr = Mon->GetSprite();
        Renderer->DrawSprite(GetTex(Spr.TextureKey), WorldX, WorldY, TileSize, TileSize, Spr);
    }

    // 플레이어 렌더링 (가장 위에 그림)
    if (!Player->IsDead())
    {
        float              WorldX = Player->GetRenderX() + TileSize * 0.5f;
        float              WorldY = Player->GetRenderY() + TileSize * 0.5f;
        const FSpriteInfo &Spr = Player->GetSprite();
        Renderer->DrawSprite(GetTex(Spr.TextureKey), WorldX, WorldY, TileSize, TileSize, Spr);
    }

    // 큐에 쌓인 RenderObjects를 먼저 처리한 뒤 암흑 오버레이
    Renderer->Render();
    if (DarknessTexture && !bDarknessDisabled && Player && !Player->IsDead())
    {
        FVec2 PlayerWorld;
        PlayerWorld.X = Player->GetRenderX() + TileSize * 0.5f;
        PlayerWorld.Y = Player->GetRenderY() + TileSize * 0.5f;
        FVec2 PlayerScreen = Camera->WorldToScreen(PlayerWorld);
        Renderer->DrawDarknessOverlay(DarknessTexture.get(), PlayerScreen.X, PlayerScreen.Y);
    }
}

// ============================================================
// 암흑 시야 텍스처 (방사형 그라데이션)
// ============================================================

static constexpr float DarknessInnerRatios[5] = {0.05f, 0.07f, 0.10f, 0.12f, 0.15f};
static constexpr float DarknessOuterRatios[5] = {0.10f, 0.13f, 0.15f, 0.17f, 0.20f};

void FStage::CreateDarknessTexture()
{
    if (!Renderer || !Renderer->Device)
        return;

    DarknessTexture.reset();

    const int   Size = 512;
    const float Center = Size * 0.5f;
    const float InnerRadius = Size * DarknessInnerRatios[DarknessLevel];
    const float OuterRadius = Size * DarknessOuterRatios[DarknessLevel];

    std::vector<uint8_t> Pixels(Size * Size * 4);

    for (int Y = 0; Y < Size; ++Y)
    {
        for (int X = 0; X < Size; ++X)
        {
            float DX = X - Center;
            float DY = Y - Center;
            float Dist = sqrtf(DX * DX + DY * DY);

            float Alpha = 0.0f;
            if (Dist <= InnerRadius)
                Alpha = 0.0f;
            else if (Dist >= OuterRadius)
                Alpha = 1.0f;
            else
                Alpha = (Dist - InnerRadius) / (OuterRadius - InnerRadius);

            // smoothstep
            Alpha = Alpha * Alpha * (3.0f - 2.0f * Alpha);

            int Idx = (Y * Size + X) * 4;
            Pixels[Idx + 0] = 0; // R
            Pixels[Idx + 1] = 0; // G
            Pixels[Idx + 2] = 0; // B
            Pixels[Idx + 3] = static_cast<uint8_t>(Alpha * 255.0f);
        }
    }

    ID3D11Texture2D          *Tex2D = nullptr;
    ID3D11ShaderResourceView *SRV = nullptr;

    D3D11_TEXTURE2D_DESC Desc = {};
    Desc.Width = Size;
    Desc.Height = Size;
    Desc.MipLevels = 1;
    Desc.ArraySize = 1;
    Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA InitData = {};
    InitData.pSysMem = Pixels.data();
    InitData.SysMemPitch = Size * 4;

    if (FAILED(Renderer->Device->CreateTexture2D(&Desc, &InitData, &Tex2D)))
        return;

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format = Desc.Format;
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels = 1;

    if (FAILED(Renderer->Device->CreateShaderResourceView(Tex2D, &SRVDesc, &SRV)))
    {
        Tex2D->Release();
        return;
    }

    DarknessTexture = std::make_unique<FTexture>(Size, Size, Tex2D, SRV);
}

// ============================================================
// 정적 배치 (타일/벽을 텍스처별로 묶어 DrawIndexed)
// ============================================================

static void BuildQuadBatch(const std::vector<std::pair<float, float>> &Centers, float Size,
                           std::vector<FVertexSimple> &OutVertices, std::vector<UINT> &OutIndices)
{
    float Half = Size * 0.5f;
    OutVertices.reserve(Centers.size() * 4);
    OutIndices.reserve(Centers.size() * 6);

    for (size_t i = 0; i < Centers.size(); ++i)
    {
        float CX = Centers[i].first;
        float CY = Centers[i].second;
        UINT  Base = static_cast<UINT>(i * 4);

        OutVertices.push_back({CX - Half, CY - Half, 0.0f, 0.0f, 0.0f}); // 좌상
        OutVertices.push_back({CX + Half, CY - Half, 0.0f, 1.0f, 0.0f}); // 우상
        OutVertices.push_back({CX + Half, CY + Half, 0.0f, 1.0f, 1.0f}); // 우하
        OutVertices.push_back({CX - Half, CY + Half, 0.0f, 0.0f, 1.0f}); // 좌하

        OutIndices.push_back(Base + 0);
        OutIndices.push_back(Base + 1);
        OutIndices.push_back(Base + 2);
        OutIndices.push_back(Base + 0);
        OutIndices.push_back(Base + 2);
        OutIndices.push_back(Base + 3);
    }
}

void FStage::BuildStaticBatches()
{
    ReleaseStaticBatches();

    // 타일을 텍스처별로 분류
    std::vector<std::pair<float, float>> FloorCenters;
    std::vector<std::pair<float, float>> GoalCenters;

    for (const auto &Tile : Tiles)
    {
        float CX = Tile.GetRenderX(TileSize) + TileSize * 0.5f;
        float CY = Tile.GetRenderY(TileSize) + TileSize * 0.5f;

        if (Tile.GetType() == ETileType::Goal)
            GoalCenters.emplace_back(CX, CY);
        else
            FloorCenters.emplace_back(CX, CY);
    }

    // Floor 배치
    if (!FloorCenters.empty())
    {
        std::vector<FVertexSimple> Verts;
        std::vector<UINT>          Idxs;
        BuildQuadBatch(FloorCenters, TileSize, Verts, Idxs);
        FloorBatch = Renderer->CreateStaticBatch(Verts.data(), (UINT)Verts.size(), Idxs.data(),
                                                 (UINT)Idxs.size());
    }

    // Goal 배치
    if (!GoalCenters.empty())
    {
        std::vector<FVertexSimple> Verts;
        std::vector<UINT>          Idxs;
        BuildQuadBatch(GoalCenters, TileSize, Verts, Idxs);
        GoalBatch = Renderer->CreateStaticBatch(Verts.data(), (UINT)Verts.size(), Idxs.data(),
                                                (UINT)Idxs.size());
    }

    // Wall 배치
    RebuildWallBatch();
}

void FStage::RebuildWallBatch()
{
    Renderer->ReleaseStaticBatch(WallBatch);

    std::vector<std::pair<float, float>> WallCenters;
    for (const auto &W : Walls)
    {
        if (W.IsDestroyed())
            continue;
        float CX = W.GetRenderX(TileSize) + TileSize * 0.5f;
        float CY = W.GetRenderY(TileSize) + TileSize * 0.5f;
        WallCenters.emplace_back(CX, CY);
    }

    if (!WallCenters.empty())
    {
        std::vector<FVertexSimple> Verts;
        std::vector<UINT>          Idxs;
        BuildQuadBatch(WallCenters, TileSize, Verts, Idxs);
        WallBatch = Renderer->CreateStaticBatch(Verts.data(), (UINT)Verts.size(), Idxs.data(),
                                                (UINT)Idxs.size());
    }
}

void FStage::ReleaseStaticBatches()
{
    if (Renderer)
    {
        Renderer->ReleaseStaticBatch(FloorBatch);
        Renderer->ReleaseStaticBatch(GoalBatch);
        Renderer->ReleaseStaticBatch(WallBatch);
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
    auto LoadTex = [&](const std::string &Key, const std::string &Path)
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
    for (auto &Tile : Tiles)
    {
        FSpriteInfo Info;
        Info.TextureKey = (Tile.GetType() == ETileType::Goal) ? "goal" : "tile_floor";
        Info.SpriteSize = {TileSize, TileSize};
        Tile.SetSprite(Info);
    }

    // 벽에 스프라이트 할당
    for (auto &W : Walls)
    {
        FSpriteInfo Info;
        Info.TextureKey = "wall";
        Info.SpriteSize = {TileSize, TileSize};
        W.SetSprite(Info);
    }

    // 플레이어 스프라이트
    {
        FSpriteInfo Info;
        Info.TextureKey = "player";
        Info.SpriteSize = {TileSize, TileSize};
        Player->SetSprite(Info);
    }

    // 몬스터 스프라이트
    for (auto &Mon : Monsters)
    {
        FSpriteInfo Info;
        Info.TextureKey = "monster";
        Info.SpriteSize = {TileSize, TileSize};
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

FActor *FStage::FindActorAt(int X, int Y)
{
    if (Player && Player->GetTileX() == X && Player->GetTileY() == Y)
    {
        return Player.get();
    }

    for (auto &Mon : Monsters)
    {
        if (Mon->GetTileX() == X && Mon->GetTileY() == Y)
        {
            return Mon.get();
        }
    }

    return nullptr;
}

const FActor *FStage::FindActorAt(int X, int Y) const
{
    if (Player && Player->GetTileX() == X && Player->GetTileY() == Y)
    {
        return Player.get();
    }

    for (const auto &Mon : Monsters)
    {
        if (Mon->GetTileX() == X && Mon->GetTileY() == Y)
        {
            return Mon.get();
        }
    }

    return nullptr;
}

FPlayer &FStage::GetPlayer() { return *Player; }

const FPlayer &FStage::GetPlayer() const { return *Player; }

void FStage::AddMonster(std::unique_ptr<FMonster> Monster)
{
    Monsters.push_back(std::move(Monster));
}

std::vector<std::unique_ptr<FMonster>> &FStage::GetMonsters() { return Monsters; }

const std::vector<std::unique_ptr<FMonster>> &FStage::GetMonsters() const { return Monsters; }

std::vector<FTile> &FStage::GetTiles() { return Tiles; }

const std::vector<FTile> &FStage::GetTiles() const { return Tiles; }

std::vector<FWall> &FStage::GetWalls() { return Walls; }

const std::vector<FWall> &FStage::GetWalls() const { return Walls; }

FWall *FStage::FindWallAt(int X, int Y)
{
    for (auto &Wall : Walls)
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
    bool bRemoved = false;
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
            bRemoved = true;
        }
        else
        {
            ++It;
        }
    }

    if (bRemoved)
    {
        RebuildWallBatch();
    }
}

FStageData &FStage::GetMap() { return *Map; }

const FStageData &FStage::GetMap() const { return *Map; }

FBeatSystem &FStage::GetBeatSystem() { return *BeatSystem; }

const FBeatSystem &FStage::GetBeatSystem() const { return *BeatSystem; }

FCamera2D &FStage::GetCamera() { return *Camera; }

const FCamera2D &FStage::GetCamera() const { return *Camera; }

FScoreSystem &FStage::GetScoreSystem() { return *ScoreSystem; }

const FScoreSystem &FStage::GetScoreSystem() const { return *ScoreSystem; }

int FStage::GetScore() const { return ScoreSystem->GetScore(); }

float FStage::GetTileSize() const { return TileSize; }

bool FStage::IsGameOver() const { return bIsGameOver; }

bool FStage::IsCleared() const { return bIsCleared; }

float FStage::GetRemainingTime() const { return RemainingTime; }

float FStage::GetTimeLimit() const { return TimeLimit; }

int FStage::GetDarknessLevel() const { return DarknessLevel; }

void FStage::SetDarknessLevel(int Level)
{
    Level = (Level < 0) ? 0 : (Level > 4) ? 4 : Level;
    if (Level == DarknessLevel)
        return;
    DarknessLevel = Level;
    CreateDarknessTexture();
}

bool FStage::IsTimeFrozen() const { return bTimeFrozen; }

void FStage::SetTimeFrozen(bool bFrozen) { bTimeFrozen = bFrozen; }

bool FStage::IsDarknessDisabled() const { return bDarknessDisabled; }

void FStage::SetDarknessDisabled(bool bDisabled) { bDarknessDisabled = bDisabled; }

int FStage::GetCurrentStageIndex() const { return CurrentStageIndex; }

const std::string &FStage::GetStageName() const { return StageName; }
