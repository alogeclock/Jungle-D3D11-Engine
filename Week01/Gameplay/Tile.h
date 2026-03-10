#pragma once

#include "SpriteInfo.h"

enum class ETileType
{
	Floor,
	Spike,
	Heal
};

class FTile
{
public:
	FTile() = default;
	FTile(int InTileX, int InTileY, ETileType InType = ETileType::Floor);

	int GetTileX() const;
	int GetTileY() const;

	float GetRenderX(float TileSize) const;
	float GetRenderY(float TileSize) const;

	void SetType(ETileType InType);
	ETileType GetType() const;

	void SetSprite(const FSpriteInfo& InSprite);
	const FSpriteInfo& GetSprite() const;

private:
	int TileX = 0;
	int TileY = 0;

	ETileType Type = ETileType::Floor;
	FSpriteInfo Sprite;
};
