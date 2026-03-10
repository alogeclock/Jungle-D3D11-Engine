#pragma once

#include "BeatSystem.h"
#include "Camera2D.h"
#include "MapData.h"
#include "Monster.h"
#include "Player.h"
#include "ScoreSystem.h"
#include "Tile.h"
#include "Wall.h"
#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>

class FActor;
class FRenderer;
class FTextureManager;
struct FSpriteInfo;
struct ID3D11Buffer;

// 스프라이트 셰이더 상수 버퍼 레이아웃
struct FSpriteConstants
{
    DirectX::XMFLOAT4X4 World;
    DirectX::XMFLOAT4X4 View;
    DirectX::XMFLOAT4X4 Projection;
    DirectX::XMFLOAT2   SpriteSize;
    DirectX::XMFLOAT2   TextureSize;
    DirectX::XMFLOAT2   SpriteOffset;
    float               IsMirrored;
    float               Pad;
};

// 스프라이트 버텍스 (위치 + UV)
struct FSpriteVertex
{
    float X, Y, Z;
    float U, V;
};

class FStage
{
  public:
    ~FStage();

    bool Load(const std::string &MapPath, FRenderer *InRenderer, FTextureManager *InTextures);
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

    FMapData       &GetMap();
    const FMapData &GetMap() const;

    FBeatSystem       &GetBeatSystem();
    const FBeatSystem &GetBeatSystem() const;

    FCamera2D       &GetCamera();
    const FCamera2D &GetCamera() const;

    FScoreSystem       &GetScoreSystem();
    const FScoreSystem &GetScoreSystem() const;

    float GetTileSize() const;

    bool IsGameOver() const;
    bool IsCleared() const;

  private:
    std::unique_ptr<FMapData>              Map;
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

    FRenderer       *Renderer = nullptr;
    FTextureManager *Textures = nullptr;

    // 렌더링 리소스
    ID3D11Buffer *QuadVB = nullptr;   // 공유 쿼드 버텍스 버퍼 (단위 사각형)
    ID3D11Buffer *SpriteCB = nullptr; // 스프라이트 상수 버퍼

    // 캐시된 View/Projection 행렬
    DirectX::XMFLOAT4X4 CachedView;
    DirectX::XMFLOAT4X4 CachedProjection;

    void CreateRenderResources();
    void ReleaseRenderResources();
    void LoadSpriteResources();
    void UpdateViewProjection();

    // 스프라이트 정보를 기반으로 텍스처를 바인딩하고 그리기
    void DrawSpriteAtWorld(float WorldCenterX, float WorldCenterY, float Width, float Height,
                           const FSpriteInfo &Sprite);
};
