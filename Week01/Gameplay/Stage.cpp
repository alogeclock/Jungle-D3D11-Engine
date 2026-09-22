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
            case ETileValue::Item:
                Tiles.emplace_back(X, Y, ETileType::Floor);
                break;
            }
        }
    }

    // 아이템 데이터 복사
    Items = Map->GetItems();

    // 메타데이터 기반 플레이어 스폰
    FSpawnPoint Spawn = Map->GetSpawnPoint();
    Player->SetPosition(Spawn.X, Spawn.Y, TileSize);

    int SpawnMonsterCount = Map->GetMonsterCount();
    Logger::Log("Get Monster Count!" + std::to_string(SpawnMonsterCount));
    int MaxTries = 1000; // 타일 탐색 시 무한 루프 방지용
    int MonsterTypeIndex = 0;

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

                // 배열을 하나씩 순회하면서 현재 생성할 몬스터를 결정
                std::vector<EMonsterType> MonsterTypeVector = Map->GetMonsterTypes();
                EMonsterType              MonsterType = EMonsterType::StoneGolem;

                if (!MonsterTypeVector.empty())
                {
                    MonsterTypeIndex =
                        MonsterTypeIndex % static_cast<int>(MonsterTypeVector.size());
                    MonsterType = MonsterTypeVector[MonsterTypeIndex];
                }

                NewMonster->SetMonsterType(MonsterType);
                NewMonster->SetMoveFrequency(3 - static_cast<int>(MonsterType));
                NewMonster->SetSearchRange(3 + 3 * static_cast<int>(MonsterType));

                AddMonster(std::move(NewMonster));
                MonsterTypeIndex++;
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

    // 암흑 시야 설정 (JSON에서 로드)
    DarknessLevel = Map->GetDarknessLevel();
    CreateDarknessTexture();

    // 제한시간 설정
    TimeLimit = Map->GetTimeLimit();
    RemainingTime = TimeLimit;

    // Angry 모드 메타데이터
    AngryTime = Map->GetAngryTime();
    AngryTimeScale = Map->GetAngryTimeScale();
    bIsAngry = false;

    // 비트 시스템 초기화 (스테이지 BPM 적용)
    BeatSystem->SetBpm(static_cast<float>(Map->GetBpm()));
    BeatSystem->Reset();
    ScoreSystem->Reset();

    // 효과음 로드
    FAudioSystem::Get().LoadWav("sfx_perfect", "Resources/Sounds/perfect.wav");
    FAudioSystem::Get().LoadWav("sfx_good", "Resources/Sounds/good.wav");
    FAudioSystem::Get().LoadWav("sfx_miss", "Resources/Sounds/miss.wav");
    FAudioSystem::Get().LoadWav("sfx_get_hit", "Resources/Sounds/get_hit.wav");

    // BGM은 LoadStage 측에서 StartBGM()으로 명시적 호출
    const std::string &MusicPath = Map->GetMusicPath();
    if (!MusicPath.empty())
    {
        BgmKey = "bgm_stage" + std::to_string(StageIndex);
        FAudioSystem::Get().LoadWav(BgmKey, MusicPath);
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
    Items.clear();
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

    // Angry 모드 체크
    if (!bIsAngry && AngryTime > 0.0f && RemainingTime <= AngryTime)
    {
        bIsAngry = true;
        BeatSystem->SetTimeScale(AngryTimeScale);
        FAudioSystem::Get().SetChannelPlaybackRate(EAudioChannel::BGM, AngryTimeScale);
        CreateAngryOverlayTexture();
    }

    // 아이템 효과 업데이트
    UpdateActiveEffects(DeltaTime);

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
        Spr.bIsMirrored = !Player->GetDefaultMirrored();
        Player->SetSprite(Spr);
    }
    else if (Context.Input.GetKeyDown(EKeyCode::Right))
    {
        MoveDir = EDirection::Right;
        bHasInput = true;
        FSpriteInfo Spr = Player->GetSprite();
        Spr.bIsMirrored = Player->GetDefaultMirrored();
        Player->SetSprite(Spr);
    }

    if (BeatSystem->ConsumeBeat())
    {
        // 몬스터 이동
        for (auto &Mon : Monsters)
        {
            Mon->OnBeat(*this);
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
                if (!Player->ConsumeInvincibility())
                    Player->Damage(1);
                // 2. 몬스터 소멸 (남은 HP만큼 데미지를 주어 IsDead() 상태로 만듦)
                (*Mon)->Damage((*Mon)->GetHp());
                FAudioSystem::Get().Play("sfx_get_hit", false);
            }

            if ((*Mon)->IsDead())
                Mon = Monsters.erase(Mon);
            else
                ++Mon;
        }
    }

    if (!Player->IsDead() && bHasInput && BeatSystem->GetBeatCount() > 1)
    {
        // 입력이 들어온 현재 시점의 박자 인덱스
        int CurrentBeatIndex = static_cast<int>(
            std::round(BeatSystem->GetElapsedTime() / BeatSystem->GetBeatInterval()));
        EBeatJudge Judge = BeatSystem->JudgeInput();

        if (Judge == EBeatJudge::Perfect)
        {
            Logger::Log("Perfect Input");
            if (Player->GetLastMovedBeatIndex() != CurrentBeatIndex)
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
            if (Player->GetLastMovedBeatIndex() != CurrentBeatIndex)
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
            if (!Player->ConsumeInvincibility())
                Player->Damage(1);
            ScoreSystem->AddBeatBonus(Judge);
            Player->SetLastMovedBeatIndex(CurrentBeatIndex);
        }
    }

    // 아이템 획득 체크
    for (auto &Item : Items)
    {
        if (!Item.bPickedUp && Player->GetTileX() == Item.X && Player->GetTileY() == Item.Y)
        {
            Item.bPickedUp = true;
            ApplyItem(Item);
            // Change tile back to PATH
            if (Map)
                Map->SetTile(Item.X, Item.Y, 0);
        }
    }

    // 비트 스킵 시 미입력 데미지
    if (BeatSystem->IsBeatSkipped())
    {
        int CurrentBeatIndex =
            static_cast<int>(BeatSystem->GetElapsedTime() / BeatSystem->GetBeatInterval());

        if (!Player->IsDead() && CurrentBeatIndex > 1)
        {
            if (Player->GetLastMovedBeatIndex() < CurrentBeatIndex)
            {
                EBeatJudge Judge = BeatSystem->JudgeInput();
                Logger::Log("No Input Detected - Player Damaged");
                if (!Player->ConsumeInvincibility())
                    Player->Damage(1);
                ScoreSystem->AddBeatBonus(EBeatJudge::Miss);
            }
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

    // 정적 배치 렌더링 (render_layers 값별 배치)
    int StageNum = GetCurrentStageIndex() + 1;
    for (int i = 0; i < MaxRenderResources; ++i)
    {
        std::string Key = "map_" + std::to_string(StageNum) + "_" + std::to_string(i + 1);
        FTexture   *Tex = Textures->Get(Key);
        if (Tex)
            Renderer->DrawBatch(RenderBatches[i], Tex);
    }

    // 텍스처 룩업 헬퍼
    auto GetTex = [&](const std::string &Key) -> FTexture *
    { return (!Key.empty() && Textures) ? Textures->Get(Key) : nullptr; };

    // 아이템 렌더링
    for (const auto &Item : Items)
    {
        if (Item.bPickedUp)
            continue;
        float       WorldX = Item.X * TileSize + TileSize * 0.5f;
        float       WorldY = Item.Y * TileSize + TileSize * 0.5f;
        std::string TexKey = GetItemTextureKey(Item.Type);
        FTexture   *Tex = Textures ? Textures->Get(TexKey) : nullptr;
        if (Tex)
        {
            FSpriteInfo Spr;
            Spr.TextureKey = TexKey;
            Renderer->DrawSprite(Tex, WorldX, WorldY, TileSize * 0.6f, TileSize * 0.6f, Spr);
        }
    }

    // 몬스터 렌더링 (Angry 모드 시 빨간색 틴트)
    DirectX::XMFLOAT4 MonsterTint = bIsAngry ? DirectX::XMFLOAT4{1.5f, 0.4f, 0.4f, 1.0f}
                                             : DirectX::XMFLOAT4{0.0f, 0.0f, 0.0f, 0.0f};

    for (const auto &Mon : Monsters)
    {
        if (Mon->IsDead())
            continue;
        float WorldX = Mon->GetRenderX() + TileSize * 0.5f;
        float WorldY = Mon->GetRenderY() + TileSize * 0.5f;

        const FSpriteInfo &Spr = bIsAngry ? Mon->GetSpriteAngry() : Mon->GetSprite();
        Renderer->DrawSprite(GetTex(Spr.TextureKey), WorldX, WorldY, TileSize, TileSize, Spr,
                             MonsterTint);
    }

    // 플레이어 렌더링 (가장 위에 그림)
    if (!Player->IsDead())
    {
        float              WorldX = Player->GetRenderX() + TileSize * 0.5f;
        float              WorldY = Player->GetRenderY() + TileSize * 0.5f;
        const FSpriteInfo &Spr = Player->GetSprite();
        Renderer->DrawSprite(GetTex(Spr.TextureKey), WorldX, WorldY, TileSize, TileSize, Spr);

        // 무적 상태일 때 쉴드 오버레이
        if (Player->HasActiveEffect(EItemType::Invincibility))
        {
            FTexture *ShieldTex = Textures->Get("effect_shield");
            if (ShieldTex)
            {
                FSpriteInfo ShieldSpr;
                Renderer->DrawSprite(ShieldTex, WorldX, WorldY, TileSize, TileSize, ShieldSpr);
            }
        }
    }

    // 암흑 오버레이 큐잉 (스프라이트 위에 렌더)
    if (DarknessTexture && !bDarknessDisabled && Player && !Player->IsDead())
    {
        FVec2 PlayerWorld;
        PlayerWorld.X = Player->GetRenderX() + TileSize * 0.5f;
        PlayerWorld.Y = Player->GetRenderY() + TileSize * 0.5f;
        FVec2 PlayerScreen = Camera->WorldToScreen(PlayerWorld);
        Renderer->DrawDarknessOverlay(DarknessTexture.get(), PlayerScreen.X, PlayerScreen.Y);
    }

    // Angry 모드 오버레이 (빨간 반투명 화면 전체)
    if (bIsAngry && AngryOverlayTexture)
    {
        float ScreenCX = Renderer->GetScreenWidth() * 0.5f;
        float ScreenCY = Renderer->GetScreenHeight() * 0.5f;
        Renderer->DrawDarknessOverlay(AngryOverlayTexture.get(), ScreenCX, ScreenCY);
    }
    // Render()는 Application에서 통합 호출
}

// ============================================================
// 암흑 시야 텍스처 (방사형 그라데이션)
// ============================================================

static constexpr float DarknessInnerRatios[5] = {0.05f, 0.07f, 0.12f, 0.15f, 0.18f};
static constexpr float DarknessOuterRatios[5] = {0.10f, 0.13f, 0.17f, 0.20f, 0.22f};

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

void FStage::CreateAngryOverlayTexture()
{
    if (!Renderer || !Renderer->Device)
        return;

    AngryOverlayTexture.reset();

    // 방사형 그라데이션: 중앙 투명 → 가장자리 빨간색 (max alpha 0.5)
    const int   Size = 512;
    const float Center = Size * 0.5f;
    const float MaxAlpha = 0.5f;

    std::vector<uint8_t> Pixels(Size * Size * 4);

    for (int Y = 0; Y < Size; ++Y)
    {
        for (int X = 0; X < Size; ++X)
        {
            float DX = (X - Center) / Center; // -1 ~ 1
            float DY = (Y - Center) / Center; // -1 ~ 1
            float Dist = sqrtf(DX * DX + DY * DY);
            if (Dist > 1.0f)
                Dist = 1.0f;

            // smoothstep
            float Alpha = Dist * Dist * (3.0f - 2.0f * Dist);
            Alpha *= MaxAlpha;

            int Idx = (Y * Size + X) * 4;
            Pixels[Idx + 0] = 180; // R
            Pixels[Idx + 1] = 20;  // G
            Pixels[Idx + 2] = 20;  // B
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

    AngryOverlayTexture = std::make_unique<FTexture>(Size, Size, Tex2D, SRV);
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

    // render_layers 값별로 셀을 분류 (0~5)
    std::vector<std::pair<float, float>> ResourceCenters[MaxRenderResources];

    for (int Y = 0; Y < Map->GetHeight(); ++Y)
    {
        for (int X = 0; X < Map->GetWidth(); ++X)
        {
            int RL = Map->GetRenderLayer(X, Y);
            if (RL < 0 || RL >= MaxRenderResources)
                continue;

            float CX = X * TileSize + TileSize * 0.5f;
            float CY = Y * TileSize + TileSize * 0.5f;
            ResourceCenters[RL].emplace_back(CX, CY);
        }
    }

    for (int i = 0; i < MaxRenderResources; ++i)
    {
        if (!ResourceCenters[i].empty())
        {
            std::vector<FVertexSimple> Verts;
            std::vector<UINT>          Idxs;
            BuildQuadBatch(ResourceCenters[i], TileSize, Verts, Idxs);
            RenderBatches[i] = Renderer->CreateStaticBatch(Verts.data(), (UINT)Verts.size(),
                                                           Idxs.data(), (UINT)Idxs.size());
        }
    }
}

void FStage::RebuildRenderBatches()
{
    if (!Renderer)
        return;

    for (int i = 0; i < MaxRenderResources; ++i)
        Renderer->ReleaseStaticBatch(RenderBatches[i]);

    std::vector<std::pair<float, float>> ResourceCenters[MaxRenderResources];

    for (int Y = 0; Y < Map->GetHeight(); ++Y)
    {
        for (int X = 0; X < Map->GetWidth(); ++X)
        {
            int RL = Map->GetRenderLayer(X, Y);
            if (RL < 0 || RL >= MaxRenderResources)
                continue;

            float CX = X * TileSize + TileSize * 0.5f;
            float CY = Y * TileSize + TileSize * 0.5f;
            ResourceCenters[RL].emplace_back(CX, CY);
        }
    }

    for (int i = 0; i < MaxRenderResources; ++i)
    {
        if (!ResourceCenters[i].empty())
        {
            std::vector<FVertexSimple> Verts;
            std::vector<UINT>          Idxs;
            BuildQuadBatch(ResourceCenters[i], TileSize, Verts, Idxs);
            RenderBatches[i] = Renderer->CreateStaticBatch(Verts.data(), (UINT)Verts.size(),
                                                           Idxs.data(), (UINT)Idxs.size());
        }
    }
}

void FStage::ReleaseStaticBatches()
{
    if (Renderer)
    {
        for (int i = 0; i < MaxRenderResources; ++i)
            Renderer->ReleaseStaticBatch(RenderBatches[i]);
    }
}

// ============================================================
// 스프라이트 리소스 관리
// ============================================================

void FStage::LoadSpriteResources()
{
    // 타일/벽 스프라이트 — render_layers 기반 배치이므로 개별 TextureKey는 참조용
    int StageNum = GetCurrentStageIndex() + 1;

    for (auto &Tile : Tiles)
    {
        FSpriteInfo Info;
        int         RL = Map->GetRenderLayer(Tile.GetTileX(), Tile.GetTileY());

        if (RL >= 0)
            Info.TextureKey = "map_" + std::to_string(StageNum) + "_" + std::to_string(RL + 1);

        Info.SpriteSize = {TileSize, TileSize};
        Tile.SetSprite(Info);
    }

    for (auto &W : Walls)
    {
        FSpriteInfo Info;
        int         RL = Map->GetRenderLayer(W.GetTileX(), W.GetTileY());
        if (RL >= 0)
            Info.TextureKey = "map_" + std::to_string(StageNum) + "_" + std::to_string(RL + 1);

        Info.SpriteSize = {TileSize, TileSize};
        W.SetSprite(Info);
    }

    // 액터에 스프라이트 + JSON 애니메이션 로드 헬퍼
    auto SetupActorAnim = [&](FActor *Actor, const std::string &TexKey)
    {
        FSpriteInfo Info;
        Info.TextureKey = TexKey;

        // JSON 애니메이션 파일 탐색: Resources/Sprites/Animation/{TexKey}.json
        std::string AnimPath = "Resources/Sprites/Animation/" + TexKey + ".json";
        bool        bDefaultMirrored = false;
        if (LoadAnimationsFromJson(AnimPath, Actor->GetAnimator(), bDefaultMirrored))
        {
            Info.bIsMirrored = bDefaultMirrored;
            Actor->SetDefaultMirrored(bDefaultMirrored);
            // JSON 로드 성공 → 첫 번째 애니메이션의 첫 프레임으로 SpriteSize 설정
            const FKeyframe *KF = Actor->GetAnimator().GetCurrentKeyframe();
            if (!KF)
            {
                // 아직 Play되지 않았으면 idle 시도
                Actor->GetAnimator().Play("idle");
                KF = Actor->GetAnimator().GetCurrentKeyframe();
            }
            if (KF)
            {
                Info.SpriteSize = {KF->OffsetMax.x - KF->OffsetMin.x,
                                   KF->OffsetMax.y - KF->OffsetMin.y};
            }
            else
            {
                FTexture *Tex = Textures ? Textures->Get(TexKey) : nullptr;
                Info.SpriteSize = Tex ? DirectX::XMFLOAT2{static_cast<float>(Tex->Width),
                                                          static_cast<float>(Tex->Height)}
                                      : DirectX::XMFLOAT2{TileSize, TileSize};
            }
        }
        else
        {
            // JSON 없음 → 텍스처 전체를 1프레임으로 사용 (기존 동작)
            FTexture *Tex = Textures ? Textures->Get(TexKey) : nullptr;
            Info.SpriteSize = Tex ? DirectX::XMFLOAT2{static_cast<float>(Tex->Width),
                                                      static_cast<float>(Tex->Height)}
                                  : DirectX::XMFLOAT2{TileSize, TileSize};
        }

        Actor->SetSprite(Info);
    };

    // 플레이어 스프라이트 + 애니메이션
    SetupActorAnim(Player.get(), PlayerSkinKey);

    // 몬스터 스프라이트 + 애니메이션
    for (auto &Mon : Monsters)
    {
        std::string TexKey = Mon->GetMonsterTextureKey(Mon->GetMonsterType());
        SetupActorAnim(Mon.get(), TexKey);
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
            // MapData도 바닥으로 변경, render_layer도 제거
            if (Map)
            {
                Map->SetTile(It->GetTileX(), It->GetTileY(), 0);
                Map->SetRenderLayer(It->GetTileX(), It->GetTileY(), -1);
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
        RebuildRenderBatches();
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

void FStage::SetIsCleared(bool bIsCleared) { this->bIsCleared = bIsCleared; }

bool FStage::IsCleared() const { return bIsCleared; }

float FStage::GetRemainingTime() const { return RemainingTime; }

void FStage::SetRemainingTime(float Time) { RemainingTime = Time; }

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

bool FStage::IsAngry() const { return bIsAngry; }

int FStage::GetCurrentStageIndex() const { return CurrentStageIndex; }

const std::string &FStage::GetStageName() const { return StageName; }

int FStage::GetGoalX() const { return GoalX; }
int FStage::GetGoalY() const { return GoalY; }

void FStage::ApplyItem(const FItemData &Item)
{
    switch (Item.Type)
    {
        // 플레이어 전용 효과 → Player에 위임
    case EItemType::Invincibility:
        if (!Player->HasActiveEffect(EItemType::Invincibility))
            Player->AddEffect({EItemType::Invincibility, 0.0f});
        break;
    case EItemType::TimeScaleDown:
        Player->AddEffect({EItemType::TimeScaleDown, Item.Duration});
        break;

        // 스테이지 전역 효과
    case EItemType::TimeFreeze:
        bTimeFrozen = true;
        TimeFreezeRemaining = Item.Duration;
        break;
    case EItemType::DarknessUp:
    {
        // 어두워짐: 레벨 -1 (항상 상대값)
        SetDarknessLevel(DarknessLevel - 1);
        break;
    }
    case EItemType::DarknessDown:
    {
        // 밝아짐: 레벨 +1 (항상 상대값)
        SetDarknessLevel(DarknessLevel + 1);
        break;
    }
    case EItemType::HpUp:
        Player->Heal(1);
        break;
    }
}

void FStage::UpdateActiveEffects(float DeltaTime)
{
    // 플레이어 효과 업데이트
    if (Player)
        Player->UpdateActiveEffects(DeltaTime);

    // 스테이지 TimeFreeze 타이머
    if (bTimeFrozen && TimeFreezeRemaining > 0.0f)
    {
        TimeFreezeRemaining -= DeltaTime;
        if (TimeFreezeRemaining <= 0.0f)
        {
            TimeFreezeRemaining = 0.0f;
            bTimeFrozen = false;
        }
    }
}

const std::vector<FItemData> &FStage::GetItems() const { return Items; }

float FStage::GetTimeFreezeRemaining() const { return TimeFreezeRemaining; }

void FStage::StartBGM()
{
    if (!BgmKey.empty())
        FAudioSystem::Get().Play(BgmKey, true, EAudioChannel::BGM);
}

void FStage::StopBGM()
{
    if (!BgmKey.empty())
        FAudioSystem::Get().Stop(BgmKey);
}

void FStage::SetPlayerSkinKey(const std::string &Key) { PlayerSkinKey = Key; }

const std::string &FStage::GetPlayerSkinKey() const { return PlayerSkinKey; }
