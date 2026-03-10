#pragma once

#include <string>
#include <vector>
#include "Core/Types.h"

class FScoreRepository
{
public:
	std::vector<FScoreRecord> Load(const std::string& Path);
	bool Save(const std::string& Path, const std::vector<FScoreRecord>& Records);

	void AddRecord(std::vector<FScoreRecord>& Records, const FScoreRecord& Record);
	void SortDescending(std::vector<FScoreRecord>& Records);
};
