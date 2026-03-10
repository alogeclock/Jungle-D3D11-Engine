#pragma once

#include <vector>
#include "../UI/Popup.h"
#include "../Core/Types.h"

class FScoreboardPopup : public IPopup
{
public:
	void SetScores(const std::vector<FScoreRecord>& InScores);

	void Open() override;
	void Close() override;
	bool IsOpen() const override;

	void Update(FGameContext& Context) override;
	void Render(FGameContext& Context) override;

private:
	bool bIsOpen = false;
	std::vector<FScoreRecord> Scores;
};
