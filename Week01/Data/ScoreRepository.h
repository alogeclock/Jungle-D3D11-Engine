#pragma once

#include <string>
#include <vector>
#include "Core/Types.h"
#include "ScoreRecord.h"

class FScoreRepository
{
  public:
    static constexpr const char *DefaultPath = "Resources/Data/scoreboard.json";

  public:
    FScoreRepository();
    explicit FScoreRepository(const std::string &InPath);

    void               SetPath(const std::string &InPath);
    const std::string &GetPath() const;

    std::vector<FScoreRecord> Load();
    std::vector<FScoreRecord> LoadSorted();

    bool Save(const std::vector<FScoreRecord> &Records);
    bool AppendRecord(const FScoreRecord &Record);

    void SortDescending(std::vector<FScoreRecord> &Records);

  private:
    std::string Path;
};
