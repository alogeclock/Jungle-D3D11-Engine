#pragma once

enum class EWallType
{
	Normal,
	Breakable,
	Boundary
};

class FWall
{
public:
	FWall() = default;
	FWall(int InTileX, int InTileY, EWallType InType = EWallType::Normal);

	int GetTileX() const;
	int GetTileY() const;

	float GetRenderX(float TileSize) const;
	float GetRenderY(float TileSize) const;

	void SetType(EWallType InType);
	EWallType GetType() const;

	bool IsBreakable() const;
	void Destroy();
	bool IsDestroyed() const;

private:
	int TileX = 0;
	int TileY = 0;

	EWallType Type = EWallType::Normal;
	bool bIsDestroyed = false;
};
