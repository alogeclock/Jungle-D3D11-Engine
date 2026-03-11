#include "pch.h"
#include <algorithm>
#include "IO/JsonFile.h"
#include "ScoreRepository.h"
#include "ThirdParty/nlohmann/json.hpp"

namespace
{
    void WriteScoreboardArray(nlohmann::json &Root, const std::vector<FScoreRecord> &Records)
    {
        Root = nlohmann::json::object();
        Root["scoreboard"] = nlohmann::json::array();

        for (const FScoreRecord &Record : Records)
        {
            Root["scoreboard"].push_back({
                {"nickname", Record.Nickname},
                {"stage", Record.Stage},
                {"score", Record.Score}
            });
        }
    }
}

FScoreRepository::FScoreRepository()
    : Path(DefaultPath)
{
}

FScoreRepository::FScoreRepository(const std::string &InPath)
    : Path(InPath)
{
}

void FScoreRepository::SetPath(const std::string &InPath)
{
    Path = InPath;
}

const std::string &FScoreRepository::GetPath() const
{
    return Path;
}

std::vector<FScoreRecord> FScoreRepository::Load()
{
    std::vector<FScoreRecord> Records;

    FJsonFile JsonFile;
    if (!JsonFile.LoadFromFile(Path))
    {
        return Records;
    }

    const nlohmann::json &Root = JsonFile.GetRoot();
    if (!Root.is_object())
    {
        return Records;
    }

    if (!Root.contains("scoreboard"))
    {
        return Records;
    }

    const nlohmann::json &Scoreboard = Root["scoreboard"];
    if (!Scoreboard.is_array())
    {
        return Records;
    }

    for (const nlohmann::json &Item : Scoreboard)
    {
        if (!Item.is_object())
            continue;

        if (!Item.contains("nickname") || !Item["nickname"].is_string())
            continue;

        if (!Item.contains("stage") || !Item["stage"].is_number_integer())
            continue;

        if (!Item.contains("score") || !Item["score"].is_number_integer())
            continue;

        FScoreRecord Record;
        Record.Nickname = Item["nickname"].get<std::string>();
        Record.Stage = Item["stage"].get<int>();
        Record.Score = Item["score"].get<int>();

        Records.push_back(Record);
    }

    return Records;
}

std::vector<FScoreRecord> FScoreRepository::LoadSorted()
{
    std::vector<FScoreRecord> Records = Load();
    SortDescending(Records);
    return Records;
}

bool FScoreRepository::Save(const std::vector<FScoreRecord> &Records)
{
    FJsonFile JsonFile;
    nlohmann::json &Root = JsonFile.GetRoot();

    WriteScoreboardArray(Root, Records);
    return JsonFile.SaveToFile(Path);
}

bool FScoreRepository::AppendRecord(const FScoreRecord &Record)
{
    std::vector<FScoreRecord> Records = Load();

    Records.push_back(Record);
    SortDescending(Records);

    return Save(Records);
}

void FScoreRepository::SortDescending(std::vector<FScoreRecord> &Records)
{
    std::sort(Records.begin(), Records.end(),
              [](const FScoreRecord &A, const FScoreRecord &B)
              {
                  if (A.Score != B.Score)
                      return A.Score > B.Score;

                  if (A.Stage != B.Stage)
                      return A.Stage > B.Stage;

                  return A.Nickname < B.Nickname;
              });
}
