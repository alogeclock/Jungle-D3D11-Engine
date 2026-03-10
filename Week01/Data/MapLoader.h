#pragma once

#include <string>
#include <vector>
#include "Core/Types.h"

class FMapData;

struct FSpawnPoint
{
	int X = 0;
	int Y = 0;
};

struct FStageInfo
{
	int Id = 0;
	std::string Name;
	int Width = 0;
	int Height = 0;
	FSpawnPoint PlayerSpawn;
	std::vector<FSpawnPoint> MonsterSpawns;
};

class FMapLoader
{
public:
	static FMapLoader& Get();

	bool Initialize(const std::string& Path);
	bool IsLoaded() const;

	int GetStageCount() const;
	bool LoadStage(int StageIndex, FMapData& OutMap, FStageInfo& OutInfo) const;

private:
	FMapLoader() = default;

	std::string FileContent;
	bool bLoaded = false;
};
