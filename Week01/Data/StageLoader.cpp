#include "pch.h"
#include <fstream>
#include <sstream>
#include "StageData.h"
#include "StageLoader.h"
#include "ThirdParty/nlohmann/json.hpp"

using json = nlohmann::json;

namespace
{
	bool ParseMonsterType(const std::string& TypeStr, EMonsterType& OutType)
	{
		if (TypeStr == "stone_golem")
		{
			OutType = EMonsterType::StoneGolem;
			return true;
		}

		if (TypeStr == "fire_golem")
		{
			OutType = EMonsterType::FireGolem;
			return true;
		}

		return false;
	}
}

FStageLoader& FStageLoader::Get()
{
	static FStageLoader Instance;
	return Instance;
}

bool FStageLoader::Initialize(const std::string& Path)
{
	if (bLoaded)
	{
		return true;
	}

	std::ifstream File(Path);
	if (!File.is_open())
	{
		return false;
	}

	std::stringstream Buffer;
	Buffer << File.rdbuf();

	FileContent = Buffer.str();
	bLoaded = true;
	return true;
}

bool FStageLoader::IsLoaded() const { return bLoaded; }

int FStageLoader::GetStageCount() const
{
	if (!bLoaded)
	{
		return 0;
	}

	try
	{
		const json Doc = json::parse(FileContent);

		if (!Doc.contains("stages") || !Doc["stages"].is_array())
		{
			return 0;
		}

		return static_cast<int>(Doc["stages"].size());
	}
	catch (...)
	{
		return 0;
	}
}

bool FStageLoader::LoadStageById(int StageIndex, FStageData& OutStage) const
{
	if (!bLoaded)
	{
		return false;
	}

	try
	{
		const json Doc = json::parse(FileContent);

		if (!Doc.contains("stages") || !Doc["stages"].is_array())
		{
			return false;
		}

		const json& Stages = Doc["stages"];
		if (StageIndex < 0 || StageIndex >= static_cast<int>(Stages.size()))
		{
			return false;
		}

		const json& Stage = Stages[StageIndex];

		if (!Stage.contains("metadata") || !Stage["metadata"].is_object())
		{
			return false;
		}

		if (!Stage.contains("layers") || !Stage["layers"].is_array())
		{
			return false;
		}

		const json& Meta = Stage["metadata"];
		const json& Layers = Stage["layers"];

		const int Width = Meta.value("width", 0);
		const int Height = Meta.value("height", 0);

		if (Width <= 0 || Height <= 0)
		{
			return false;
		}

		if (static_cast<int>(Layers.size()) != Height)
		{
			return false;
		}

		OutStage.Clear();
		OutStage.SetStageId(Meta.value("id", 0));
		OutStage.SetStageName(Meta.value("name", std::string{}));
		OutStage.Resize(Width, Height);

		OutStage.SetBpm(Meta.value("bpm", 120));
		OutStage.SetMusicPath(Meta.value("music", std::string{}));
		OutStage.SetTimeLimit(Meta.value("time_limit", 60.0f));
		OutStage.SetMonsterCount(Meta.value("monster_count", 0));
        OutStage.SetIntroMessage(Meta.value("intro_message", std::string{}));

		if (Meta.contains("spawn_point") && Meta["spawn_point"].is_object())
		{
			const json& SpawnPoint = Meta["spawn_point"];
			const int   SpawnX = SpawnPoint.value("x", 0);
			const int   SpawnY = SpawnPoint.value("y", 0);
			OutStage.SetSpawnPoint(SpawnX, SpawnY);
		}
		else
		{
			OutStage.SetSpawnPoint(0, 0);
		}

		if (Meta.contains("monster_types"))
		{
			if (!Meta["monster_types"].is_array())
			{
				return false;
			}

			for (const auto& TypeJson : Meta["monster_types"])
			{
				if (!TypeJson.is_string())
				{
					return false;
				}

				EMonsterType MonsterType;
				if (!ParseMonsterType(TypeJson.get<std::string>(), MonsterType))
				{
					return false;
				}

				OutStage.AddMonsterType(MonsterType);
			}
		}

		if (Meta.contains("items") && Meta["items"].is_array())
		{
			for (const auto& ItemJson : Meta["items"])
			{
				FItemData Item;
				Item.X = ItemJson.value("x", 0);
				Item.Y = ItemJson.value("y", 0);

				std::string TypeStr = ItemJson.value("type", std::string{});
				if (TypeStr == "invincibility")
					Item.Type = EItemType::Invincibility;
				else if (TypeStr == "time_scale_up")
					Item.Type = EItemType::TimeScaleUp;
				else if (TypeStr == "time_scale_down")
					Item.Type = EItemType::TimeScaleDown;
				else if (TypeStr == "time_freeze")
					Item.Type = EItemType::TimeFreeze;
				else if (TypeStr == "darkness_up")
					Item.Type = EItemType::DarknessUp;
				else if (TypeStr == "darkness_down")
					Item.Type = EItemType::DarknessDown;

				Item.Duration = ItemJson.value("duration", 5.0f);
				Item.Level = ItemJson.value("level", 0);

				OutStage.AddItem(Item);
			}
		}

		for (int Y = 0; Y < Height; ++Y)
		{
			const json& Row = Layers[Y];

			if (!Row.is_array() || static_cast<int>(Row.size()) != Width)
			{
				return false;
			}

			for (int X = 0; X < Width; ++X)
			{
				if (!Row[X].is_number_integer())
				{
					return false;
				}

				OutStage.SetTile(X, Y, Row[X].get<int>());
			}
		}

		return true;
	}
	catch (...)
	{
		return false;
	}
}
