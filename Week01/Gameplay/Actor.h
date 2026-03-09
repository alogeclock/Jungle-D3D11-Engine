#pragma once

#include "../Core/Types.h"

class FStage;

class FActor
{
public:
	virtual ~FActor() = default;

	virtual EActorType GetActorType() const = 0;

	virtual void Update(float DeltaTime);
	virtual void OnBeat(FStage& Stage) = 0;

	void SetPosition(int InTileX, int InTileY, float TileSize);
	bool TryMove(FStage& Stage, EDirection Direction);
	void StartMoveTo(int InTileX, int InTileY, float TileSize);

	int GetTileX() const;
	int GetTileY() const;

	float GetRenderX() const;
	float GetRenderY() const;

	void SetDirection(EDirection InDirection);
	EDirection GetDirection() const;

	void SetHp(int InHp);
	int GetHp() const;

	void Damage(int Amount);
	void Heal(int Amount);
	bool IsDead() const;

	bool IsMoving() const;

protected:
	int TileX = 0;
	int TileY = 0;

	float RenderX = 0.0f;
	float RenderY = 0.0f;

	EDirection Direction = EDirection::Down;

	int Hp = 1;

	bool bIsMoving = false;
	float MoveTimer = 0.0f;
	float MoveDuration = 0.12f;

	float MoveStartX = 0.0f;
	float MoveStartY = 0.0f;
	float MoveTargetX = 0.0f;
	float MoveTargetY = 0.0f;
};
