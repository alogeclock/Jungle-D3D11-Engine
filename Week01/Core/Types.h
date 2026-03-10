#pragma once

#include <string>

// todo: 수학 모듈 별도 생성
// todo: vector3로 명명 (차원 명시)

struct FVertexSimple
{
    float x, y, z; // Position
    float u, v;
};

struct FVector
{
    float x, y, z;
    FVector(float _x = 0, float _y = 0, float _z = 0) : x(_x), y(_y), z(_z) {}
};

enum class ESceneType
{
    Title,
    Play
};

enum class EDirection
{
    Up,
    Down,
    Left,
    Right
};

enum class ETileValue
{
    Path = 0,
    Wall = 1,
    Outer = 2,
    Goal = 3
};

enum class EBeatJudge
{
    Perfect,
    Good,
    Miss
};

enum class EActorType
{
    Player,
    Monster
};

enum class EMonsterAIType
{
    ChasePlayer,
    RandomMove,
    Patrol
};

enum class EKeyCode
{
    Up,
    Down,
    Left,
    Right,
    Count
};

struct FVec2
{
    float X = 0.0f;
    float Y = 0.0f;
};

struct FScoreRecord
{
    std::string Name;
    int         Stage = 0;
    int         Score = 0;
};

enum class TileType : int
{
    Path = 0,  // 이동 가능 (캐릭터가 서 있을 수 있는 곳)
    Wall = 1,  // 이동 불가 (벽)
    Outer = 2, // 이동 불가 (바깥 여백/낭떠러지 등)
    Goal = 3,  // 스테이지 클리어 지점 (도착 시 다음 맵으로)
    Count = 4
};
