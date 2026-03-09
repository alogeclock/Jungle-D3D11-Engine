#include "Wall.h"

FWall::FWall(int InTileX, int InTileY, EWallType InType)
	: TileX(InTileX), TileY(InTileY), Type(InType)
{
}

int FWall::GetTileX() const
{
	return TileX;
}

int FWall::GetTileY() const
{
	return TileY;
}

float FWall::GetRenderX(float TileSize) const
{
	return static_cast<float>(TileX) * TileSize;
}

float FWall::GetRenderY(float TileSize) const
{
	return static_cast<float>(TileY) * TileSize;
}

void FWall::SetType(EWallType InType)
{
	Type = InType;
}

EWallType FWall::GetType() const
{
	return Type;
}

bool FWall::IsBreakable() const
{
	return Type == EWallType::Breakable;
}

void FWall::Destroy()
{
	if (IsBreakable())
	{
		bIsDestroyed = true;
	}
}

bool FWall::IsDestroyed() const
{
	return bIsDestroyed;
}
