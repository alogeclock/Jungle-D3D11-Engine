#include "Tile.h"

FTile::FTile(int InTileX, int InTileY, ETileType InType)
	: TileX(InTileX), TileY(InTileY), Type(InType)
{
}

int FTile::GetTileX() const
{
	return TileX;
}

int FTile::GetTileY() const
{
	return TileY;
}

float FTile::GetRenderX(float TileSize) const
{
	return static_cast<float>(TileX) * TileSize;
}

float FTile::GetRenderY(float TileSize) const
{
	return static_cast<float>(TileY) * TileSize;
}

void FTile::SetType(ETileType InType)
{
	Type = InType;
}

ETileType FTile::GetType() const
{
	return Type;
}

void FTile::SetSprite(const FSpriteInfo& InSprite)
{
	Sprite = InSprite;
}

const FSpriteInfo& FTile::GetSprite() const
{
	return Sprite;
}
