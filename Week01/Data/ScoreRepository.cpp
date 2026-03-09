#include "ScoreRepository.h"
#include <algorithm>

std::vector<FScoreRecord> FScoreRepository::Load(const std::string& Path)
{
	std::vector<FScoreRecord> Records;
	return Records;
}

bool FScoreRepository::Save(const std::string& Path, const std::vector<FScoreRecord>& Records)
{
	return false;
}

void FScoreRepository::AddRecord(std::vector<FScoreRecord>& Records, const FScoreRecord& Record)
{
	Records.push_back(Record);
}

void FScoreRepository::SortDescending(std::vector<FScoreRecord>& Records)
{
	std::sort(Records.begin(), Records.end(),
		[](const FScoreRecord& A, const FScoreRecord& B)
		{
			return A.Score > B.Score;
		});
}
