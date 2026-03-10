#include <algorithm>
#include "IO/JsonFile.h"
#include "ScoreRepository.h"

std::vector<FScoreRecord> FScoreRepository::Load(const std::string &Path)
{
    std::vector<FScoreRecord> Records;

    FJsonFile JsonFile;
    if (!JsonFile.LoadFromFile(Path))
    {
        return Records;
    }

    const nlohmann::json &Root = JsonFile.GetRoot();
    if (!Root.contains("scoreboard") || !Root["scoreboard"].is_array())
    {
        return Records;
    }

    for (const nlohmann::json &Item : Root["scoreboard"])
    {
        if (!Item.is_object())
        {
            continue;
        }

        if (!Item.contains("nickname") || !Item["nickname"].is_string())
        {
            continue;
        }

        if (!Item.contains("stage") || !Item["stage"].is_number_integer())
        {
            continue;
        }

        if (!Item.contains("score") || !Item["score"].is_number_integer())
        {
            continue;
        }

        FScoreRecord Record;
        Record.Name = Item["nickname"].get<std::string>();
        Record.Stage = Item["stage"].get<int>();
        Record.Score = Item["score"].get<int>();

        Records.push_back(Record);
    }

    return Records;
}

bool FScoreRepository::Save(const std::string &Path, const std::vector<FScoreRecord> &Records)
{
    FJsonFile       JsonFile;
    nlohmann::json &Root = JsonFile.GetRoot();

    Root = nlohmann::json::object();
    Root["scoreboard"] = nlohmann::json::array();

    for (const FScoreRecord &Record : Records)
    {
        Root["scoreboard"].push_back(
            {{"nickname", Record.Name}, {"stage", Record.Stage}, {"score", Record.Score}});
    }

    return JsonFile.SaveToFile(Path);
}

void FScoreRepository::AddRecord(std::vector<FScoreRecord> &Records, const FScoreRecord &Record)
{
    Records.push_back(Record);
}

void FScoreRepository::SortDescending(std::vector<FScoreRecord> &Records)
{
    std::sort(Records.begin(), Records.end(),
              [](const FScoreRecord &A, const FScoreRecord &B)
              {
                  if (A.Score != B.Score)
                      return A.Score > B.Score;
                  return A.Stage > B.Stage;
              });
}
