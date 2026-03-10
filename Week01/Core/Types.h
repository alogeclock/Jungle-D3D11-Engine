#pragma once

#include <string>

// ---- Legacy types (used by URenderer / main.cpp) ----

struct FVertexSimple
{
	float x, y, z;    // Position
	float r, g, b, a; // Color
	float u, v;
};

struct FVector
{
	float x, y, z;
	FVector(float _x = 0, float _y = 0, float _z = 0) : x(_x), y(_y), z(_z) {}
};


// ---- New types ----

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
	Enter,
	Esc
};

struct FVec2
{
	float X = 0.0f;
	float Y = 0.0f;
};

struct FScoreRecord
{
	std::string Name;
	int Score = 0;
};
