#include "ScoreboardPopup.h"
#include "../Core/GameContext.h"

void FScoreboardPopup::SetScores(const std::vector<FScoreRecord>& InScores)
{
	Scores = InScores;
}

void FScoreboardPopup::Open()
{
	bIsOpen = true;
}

void FScoreboardPopup::Close()
{
	bIsOpen = false;
}

bool FScoreboardPopup::IsOpen() const
{
	return bIsOpen;
}

void FScoreboardPopup::Update(FGameContext& Context)
{
}

void FScoreboardPopup::Render(FGameContext& Context)
{
}
