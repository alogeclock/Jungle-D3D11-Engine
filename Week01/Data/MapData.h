#pragma once

#include <vector>
#include "Core/Types.h"

class FMapData
{
public:
	void Clear();
	void Resize(int InWidth, int InHeight);

	void SetTile(int X, int Y, int Value);
	int GetTile(int X, int Y) const;

	bool IsInside(int X, int Y) const;
	bool IsWalkable(int X, int Y) const;
	bool IsGoal(int X, int Y) const;

	int GetWidth() const;
	int GetHeight() const;

	float GetWorldWidth(float TileSize) const;
	float GetWorldHeight(float TileSize) const;

private:
	std::vector<std::vector<int>> Tiles;
	int Width = 0;
	int Height = 0;
};
