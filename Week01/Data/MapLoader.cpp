#include "MapData.h"
#include "MapLoader.h"

#include <fstream>
#include <sstream>
#include "ThirdParty/nlohmann/json.hpp"

using json = nlohmann::json;

FMapLoader& FMapLoader::Get()
{
	static FMapLoader Instance;
	return Instance;
}

bool FMapLoader::Initialize(const std::string& Path)
{
	if (bLoaded)
		return true;

	std::ifstream File(Path);
	if (!File.is_open())
		return false;

	std::stringstream Buffer;
	Buffer << File.rdbuf();
	FileContent = Buffer.str();
	bLoaded = true;
	return true;
}

bool FMapLoader::IsLoaded() const
{
	return bLoaded;
}

int FMapLoader::GetStageCount() const
{
	if (!bLoaded) return 0;

	try
	{
		json Doc = json::parse(FileContent);
		if (Doc.contains("stages") && Doc["stages"].is_array())
		{
			return static_cast<int>(Doc["stages"].size());
		}
	}
	catch (...)
	{
	}
	return 0;
}

bool FMapLoader::LoadStage(int StageIndex, FMapData& OutMap, FStageInfo& OutInfo) const
{
	if (!bLoaded) return false;

	try
	{
		json Doc = json::parse(FileContent);

		if (!Doc.contains("stages") || !Doc["stages"].is_array())
			return false;

		auto& Stages = Doc["stages"];
		if (StageIndex < 0 || StageIndex >= static_cast<int>(Stages.size()))
			return false;

		auto& Stage = Stages[StageIndex];
		auto& Meta = Stage["metadata"];

		OutInfo.Id = Meta.value("id", 0);
		OutInfo.Name = Meta.value("name", "");
		OutInfo.Width = Meta.value("width", 0);
		OutInfo.Height = Meta.value("height", 0);

		if (OutInfo.Width <= 0 || OutInfo.Height <= 0)
			return false;

		if (Meta.contains("spawn_point"))
		{
			OutInfo.PlayerSpawn.X = Meta["spawn_point"].value("x", 0);
			OutInfo.PlayerSpawn.Y = Meta["spawn_point"].value("y", 0);
		}

		OutInfo.MonsterSpawns.clear();
		if (Meta.contains("monster_spawns") && Meta["monster_spawns"].is_array())
		{
			for (auto& Spawn : Meta["monster_spawns"])
			{
				FSpawnPoint Sp;
				Sp.X = Spawn.value("x", 0);
				Sp.Y = Spawn.value("y", 0);
				OutInfo.MonsterSpawns.push_back(Sp);
			}
		}

		auto& Layers = Stage["layers"];
		if (!Layers.is_array() || static_cast<int>(Layers.size()) != OutInfo.Height)
			return false;

		OutMap.Resize(OutInfo.Width, OutInfo.Height);

		for (int Y = 0; Y < OutInfo.Height; Y++)
		{
			auto& Row = Layers[Y];
			if (!Row.is_array() || static_cast<int>(Row.size()) != OutInfo.Width)
				return false;

			for (int X = 0; X < OutInfo.Width; X++)
			{
				OutMap.SetTile(X, Y, Row[X].get<int>());
			}
		}

		return true;
	}
	catch (...)
	{
		return false;
	}
}
