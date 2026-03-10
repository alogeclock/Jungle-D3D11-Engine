#include "Stage.h"
#include "Core/GameContext.h"
#include "Core/Input.h"
#include "Core/Logger.h"
#include "BeatSystem.h"
#include "Camera2D.h"
#include "Render/Renderer.h"
#include "Render/Texture.h"
#include "Render/TextureManager.h"
#include "Data/MapLoader.h"
#include "Data/MapData.h"
#include "IO/ImageLoader.h"
#include "Monster.h"
#include "Player.h"
#include "ScoreSystem.h"
#include "SpriteInfo.h"

using namespace DirectX;

FStage::~FStage() { ReleaseRenderResources(); }

bool FStage::Load(int StageIndex, FRenderer *InRenderer, FTextureManager *InTextures)
{
    Renderer = InRenderer;
    Textures = InTextures;

    Map = std::make_unique<FMapData>();
    Player = std::make_unique<FPlayer>();
    BeatSystem = std::make_unique<FBeatSystem>();
    Camera = std::make_unique<FCamera2D>();
    ScoreSystem = std::make_unique<FScoreSystem>();

    // 싱글턴 MapLoader에서 스테이지 로드
    FStageInfo StageInfo;
    if (!FMapLoader::Get().LoadStage(StageIndex, *Map, StageInfo))
    {
        return false;
    }

    CurrentStageIndex = StageIndex;
    StageName = StageInfo.Name;

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
            int TileVal = Map->GetTile(X, Y);
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
    Player->SetPosition(StageInfo.PlayerSpawn.X, StageInfo.PlayerSpawn.Y, TileSize);

    // 메타데이터 기반 몬스터 스폰
    for (const auto& Spawn : StageInfo.MonsterSpawns)
    {
        auto Mon = std::make_unique<FMonster>();
        Mon->SetPosition(Spawn.X, Spawn.Y, TileSize);
        Monsters.push_back(std::move(Mon));
    }

    // 카메라 설정
    Camera->SetWorldBounds(Map->GetWorldWidth(TileSize), Map->GetWorldHeight(TileSize));
    Camera->SetViewportSize(Renderer->ViewportInfo.Width, Renderer->ViewportInfo.Height);

    // 스프라이트 리소스 로드 + 엔티티에 스프라이트 할당
    LoadSpriteResources();

    // 렌더링 리소스 생성
    CreateRenderResources();

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
    }
    else if (Context.Input.GetKeyDown(EKeyCode::Right))
    {
        MoveDir = EDirection::Right;
        bHasInput = true;
    }

    if (!Player->IsDead() && bHasInput)
    {
        // 입력이 들어온 현재 시점의 박자 인덱스
        int CurrentBeatIndex =
            static_cast<int>(BeatSystem->GetElapsedTime() / BeatSystem->GetBeatInterval());

        if (BeatSystem->JudgeInput() == EBeatJudge::Good) //
        {
            Logger::Log("Good Input");
            if (Player->GetLastMovedBeatIndex() == CurrentBeatIndex)
            {
                Player->Damage(1); // 한 박자 내 중복 이동 시 데미지
            }
            else
            {
                // 즉시 이동 처리
                Player->QueueInput(MoveDir); //
                Player->OnBeat(*this);       //
                Player->SetLastMovedBeatIndex(CurrentBeatIndex);
            }
        }
        else
        {
            Logger::Log("Miss Input");
            Player->Damage(1); // 엇박자 입력 시 데미지
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
                Player->Damage(1); //
            }
        }

        // 몬스터 이동
        for (auto &Mon : Monsters)
        {
            Mon->OnBeat(*this); //
        }
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
    if (!Renderer || !QuadVB || !SpriteCB)
        return;

    // View/Projection 행렬 갱신
    UpdateViewProjection();

    // 버텍스 버퍼, 상수 버퍼 바인딩
    UINT stride = sizeof(FSpriteVertex);
    UINT offset = 0;
    Renderer->DeviceContext->IASetVertexBuffers(0, 1, &QuadVB, &stride, &offset);
    Renderer->DeviceContext->VSSetConstantBuffers(0, 1, &SpriteCB);
    Renderer->DeviceContext->PSSetConstantBuffers(0, 1, &SpriteCB);

    // 바닥 타일 렌더링
    for (const auto &Tile : Tiles)
    {
        float WorldX = Tile.GetRenderX(TileSize) + TileSize * 0.5f;
        float WorldY = Tile.GetRenderY(TileSize) + TileSize * 0.5f;
        DrawSpriteAtWorld(WorldX, WorldY, TileSize, TileSize, Tile.GetSprite());
    }

    // 벽 렌더링
    for (const auto &W : Walls)
    {
        if (W.IsDestroyed())
            continue;
        float WorldX = W.GetRenderX(TileSize) + TileSize * 0.5f;
        float WorldY = W.GetRenderY(TileSize) + TileSize * 0.5f;
        DrawSpriteAtWorld(WorldX, WorldY, TileSize, TileSize, W.GetSprite());
    }

    // 몬스터 렌더링
    for (const auto &Mon : Monsters)
    {
        if (Mon->IsDead())
            continue;
        float WorldX = Mon->GetRenderX() + TileSize * 0.5f;
        float WorldY = Mon->GetRenderY() + TileSize * 0.5f;
        DrawSpriteAtWorld(WorldX, WorldY, TileSize, TileSize, Mon->GetSprite());
    }

    // 플레이어 렌더링 (가장 위에 그림)
    {
        if (Player->IsDead())
            return;
        float WorldX = Player->GetRenderX() + TileSize * 0.5f;
        float WorldY = Player->GetRenderY() + TileSize * 0.5f;
        DrawSpriteAtWorld(WorldX, WorldY, TileSize, TileSize, Player->GetSprite());
    }
}

// ============================================================
// 렌더링 리소스 관리
// ============================================================

void FStage::CreateRenderResources()
{
    ReleaseRenderResources();

    auto *Device = Renderer->Device;

    // 단위 쿼드 버텍스 버퍼 생성 (중심 기준 -0.5 ~ 0.5, UV 포함)
    FSpriteVertex Vertices[] = {
        {-0.5f, -0.5f, 0.0f, 0.0f, 0.0f}, // 좌상
        {0.5f, -0.5f, 0.0f, 1.0f, 0.0f},  // 우상
        {0.5f, 0.5f, 0.0f, 1.0f, 1.0f},   // 우하

        {-0.5f, -0.5f, 0.0f, 0.0f, 0.0f}, // 좌상
        {0.5f, 0.5f, 0.0f, 1.0f, 1.0f},   // 우하
        {-0.5f, 0.5f, 0.0f, 0.0f, 1.0f},  // 좌하
    };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(Vertices);
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {Vertices};
    Device->CreateBuffer(&vbDesc, &vbData, &QuadVB);

    // 스프라이트 상수 버퍼 생성
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = (sizeof(FSpriteConstants) + 0xF) & ~0xF; // 16바이트 정렬
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    Device->CreateBuffer(&cbDesc, nullptr, &SpriteCB);
}

void FStage::ReleaseRenderResources()
{
    if (QuadVB)
    {
        QuadVB->Release();
        QuadVB = nullptr;
    }
    if (SpriteCB)
    {
        SpriteCB->Release();
        SpriteCB = nullptr;
    }
}

void FStage::UpdateViewProjection()
{
    FVec2 CamPos = Camera->GetPosition();
    float Zoom = Camera->GetZoom();
    float VpW = Renderer->ViewportInfo.Width;
    float VpH = Renderer->ViewportInfo.Height;

    // View: 카메라 위치만큼 이동 (줌은 Projection에서 처리하거나 View에서 처리)
    // 카메라 좌표 빼기 → 줌 스케일링
    XMMATRIX View =
        XMMatrixTranslation(-CamPos.X, -CamPos.Y, 0.0f) * XMMatrixScaling(Zoom, Zoom, 1.0f);

    // Projection: 직교 투영 (픽셀 좌표 → NDC)
    // Left=0, Right=VpW, Bottom=VpH, Top=0 → Y 아래로 증가 (2D 게임 좌표계)
    XMMATRIX Proj = XMMatrixOrthographicOffCenterLH(0.0f, VpW, VpH, 0.0f, 0.0f, 1.0f);

    XMStoreFloat4x4(&CachedView, XMMatrixTranspose(View));
    XMStoreFloat4x4(&CachedProjection, XMMatrixTranspose(Proj));
}

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
            if (Tex) Textures->Register(Key, std::move(Tex));
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

void FStage::DrawSpriteAtWorld(float WorldCenterX, float WorldCenterY, float Width, float Height,
                               const FSpriteInfo &Sprite)
{
    // 텍스처 바인딩
    XMFLOAT2 TexSize = {1.0f, 1.0f};

    if (Textures && !Sprite.TextureKey.empty())
    {
        FTexture *Tex = Textures->Get(Sprite.TextureKey);
        if (Tex && Tex->GetTextureSRV())
        {
            ID3D11ShaderResourceView* SRV = Tex->GetTextureSRV();
            Renderer->DeviceContext->PSSetShaderResources(0, 1, &SRV);
            TexSize = {static_cast<float>(Tex->Width), static_cast<float>(Tex->Height)};
        }
    }

    // World 행렬: 크기 스케일링 → 월드 위치로 이동
    XMMATRIX World = XMMatrixScaling(Width, Height, 1.0f) *
                     XMMatrixTranslation(WorldCenterX, WorldCenterY, 0.0f);

    // 상수 버퍼 업데이트
    FSpriteConstants CB = {};
    XMStoreFloat4x4(&CB.World, XMMatrixTranspose(World));
    CB.View = CachedView;
    CB.Projection = CachedProjection;
    // sprite atlas를 사용하지 않으므로 전체 텍스처 UV 사용
    CB.SpriteSize = TexSize;
    CB.TextureSize = TexSize;
    CB.SpriteOffset = Sprite.SpriteOffset;
    CB.IsMirrored = Sprite.bIsMirrored ? 1.0f : 0.0f;

    D3D11_MAPPED_SUBRESOURCE Mapped;
    Renderer->DeviceContext->Map(SpriteCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
    memcpy(Mapped.pData, &CB, sizeof(CB));
    Renderer->DeviceContext->Unmap(SpriteCB, 0);

    // 드로우
    Renderer->DeviceContext->Draw(6, 0);
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

FMapData &FStage::GetMap() { return *Map; }

const FMapData &FStage::GetMap() const { return *Map; }

FBeatSystem &FStage::GetBeatSystem() { return *BeatSystem; }

const FBeatSystem &FStage::GetBeatSystem() const { return *BeatSystem; }

FCamera2D &FStage::GetCamera() { return *Camera; }

const FCamera2D &FStage::GetCamera() const { return *Camera; }

FScoreSystem &FStage::GetScoreSystem() { return *ScoreSystem; }

const FScoreSystem &FStage::GetScoreSystem() const { return *ScoreSystem; }

float FStage::GetTileSize() const { return TileSize; }

bool FStage::IsGameOver() const { return bIsGameOver; }

bool FStage::IsCleared() const { return bIsCleared; }

int FStage::GetCurrentStageIndex() const { return CurrentStageIndex; }

const std::string& FStage::GetStageName() const { return StageName; }
