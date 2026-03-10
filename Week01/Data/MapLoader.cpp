#include "JsonFile.h"
#include "MapData.h"
#include "MapLoader.h"

#include <fstream>
#include <sstream>

using json = nlohmann::json;

namespace
{
    bool GetStages(const json &Root, const json *&OutStages)
    {
        if (!Root.is_object())
            return false;

        if (!Root.contains("stages") || !Root["stages"].is_array())
            return false;

        OutStages = &Root["stages"];
        return true;
    }

    bool ParseMetadata(const json &Metadata, FMapData &OutMap)
    {
        if (!Metadata.is_object())
            return false;

        if (!Metadata.contains("width") || !Metadata["width"].is_number_integer())
            return false;

        if (!Metadata.contains("height") || !Metadata["height"].is_number_integer())
            return false;

        const int Width = Metadata["width"].get<int>();
        const int Height = Metadata["height"].get<int>();

        if (Width <= 0 || Height <= 0)
            return false;

        OutMap.Resize(Width, Height);

        if (Metadata.contains("id") && Metadata["id"].is_number_integer())
            OutMap.SetStageId(Metadata["id"].get<int>());

        if (Metadata.contains("name") && Metadata["name"].is_string())
            OutMap.SetStageName(Metadata["name"].get<std::string>());

        if (Metadata.contains("spawn_point") && Metadata["spawn_point"].is_object())
        {
            const json &SpawnPoint = Metadata["spawn_point"];

            if (SpawnPoint.contains("x") && SpawnPoint["x"].is_number_integer() &&
                SpawnPoint.contains("y") && SpawnPoint["y"].is_number_integer())
            {
                OutMap.SetSpawnPoint(SpawnPoint["x"].get<int>(), SpawnPoint["y"].get<int>());
            }
        }

        return true;
    }

    bool ParseLayers(const json &Layers, FMapData &OutMap)
    {
        if (!Layers.is_array())
            return false;

        if (static_cast<int>(Layers.size()) != OutMap.GetHeight())
            return false;

        for (int Y = 0; Y < OutMap.GetHeight(); ++Y)
        {
            const json &Row = Layers[Y];
            if (!Row.is_array() || static_cast<int>(Row.size()) != OutMap.GetWidth())
                return false;

            for (int X = 0; X < OutMap.GetWidth(); ++X)
            {
                if (!Row[X].is_number_integer())
                    return false;

                OutMap.SetTile(X, Y, Row[X].get<int>());
            }
        }

        return true;
    }

    bool ParseStage(const json &Stage, FMapData &OutMap)
    {
        OutMap.Clear();

        if (!Stage.is_object())
            return false;

        if (!Stage.contains("metadata") || !Stage["metadata"].is_object())
            return false;

        if (!Stage.contains("layers") || !Stage["layers"].is_array())
            return false;

        if (!ParseMetadata(Stage["metadata"], OutMap))
        {
            OutMap.Clear();
            return false;
        }

        if (!ParseLayers(Stage["layers"], OutMap))
        {
            OutMap.Clear();
            return false;
        }

        return true;
    }
} // namespace

bool FMapLoader::LoadFromFile(FMapData &OutMap)
{
    OutMap.Clear();

    FJsonFile JsonFile;
    if (!JsonFile.LoadFromFile(Path))
        return false;

    const json &Root = JsonFile.GetRoot();

    const json *Stages = nullptr;
    if (!GetStages(Root, Stages))
        return false;

    if (Stages->empty())
        return false;

    return ParseStage((*Stages)[0], OutMap);
}

bool FMapLoader::LoadStageById(int StageId, FMapData &OutMap)
{
    OutMap.Clear();

    FJsonFile JsonFile;
    if (!JsonFile.LoadFromFile(Path))
        return false;

    const json &Root = JsonFile.GetRoot();

    const json *Stages = nullptr;
    if (!GetStages(Root, Stages))
        return false;

    for (const json &Stage : *Stages)
    {
        if (!Stage.is_object())
            continue;

        if (!Stage.contains("metadata") || !Stage["metadata"].is_object())
            continue;

        const json &Metadata = Stage["metadata"];
        if (!Metadata.contains("id") || !Metadata["id"].is_number_integer())
            continue;

        if (Metadata["id"].get<int>() == StageId)
            return ParseStage(Stage, OutMap);
    }

    return false;
}

int FMapLoader::GetStageCount()
{
    FJsonFile JsonFile;
    if (!JsonFile.LoadFromFile(Path))
        return 0;

    const json &Root = JsonFile.GetRoot();

    const json *Stages = nullptr;
    if (!GetStages(Root, Stages))
        return 0;

    return static_cast<int>(Stages->size());
}
