#pragma once

#include <vector>
#include "ScoreRecord.h"
#include "Core/Application.h"

namespace ScoreRepository
{
    inline constexpr const char *DefaultPath ="Resources/Data/scoreboard.json";

    std::vector<FScoreRecord> Load();
    std::vector<FScoreRecord> LoadSorted();

    bool Save(const std::vector<FScoreRecord> &Records);
    bool AppendRecord(const FScoreRecord &Record);

    void SortDescending(std::vector<FScoreRecord> &Records);
} // namespace ScoreRepository