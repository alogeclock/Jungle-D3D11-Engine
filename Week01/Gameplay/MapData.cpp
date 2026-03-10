#include "MapData.h"

void FMapData::Clear()
{
	Tiles.clear();
	Width = 0;
	Height = 0;
}

void FMapData::Resize(int InWidth, int InHeight)
{
	Width = InWidth;
	Height = InHeight;
	Tiles.assign(Height, std::vector<int>(Width, 0));
}

void FMapData::SetTile(int X, int Y, int Value)
{
	if (IsInside(X, Y))
	{
		Tiles[Y][X] = Value;
	}
}

int FMapData::GetTile(int X, int Y) const
{
	if (IsInside(X, Y))
	{
		return Tiles[Y][X];
	}
	return -1;
}

bool FMapData::IsInside(int X, int Y) const
{
	return X >= 0 && X < Width && Y >= 0 && Y < Height;
}

bool FMapData::IsWalkable(int X, int Y) const
{
	if (!IsInside(X, Y))
	{
		return false;
	}
	int V = Tiles[Y][X];
	return V == static_cast<int>(ETileValue::Path) || V == static_cast<int>(ETileValue::Goal);
}

bool FMapData::IsGoal(int X, int Y) const
{
	if (!IsInside(X, Y))
	{
		return false;
	}
	return Tiles[Y][X] == static_cast<int>(ETileValue::Goal);
}

int FMapData::GetWidth() const
{
	return Width;
}

int FMapData::GetHeight() const
{
	return Height;
}

float FMapData::GetWorldWidth(float TileSize) const
{
	return static_cast<float>(Width) * TileSize;
}

float FMapData::GetWorldHeight(float TileSize) const
{
	return static_cast<float>(Height) * TileSize;
}
