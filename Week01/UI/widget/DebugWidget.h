#pragma once

#include <functional>
#include "UIWidget.h"

class FStage;

class FDebugWidget : public IUIWidget
{
public:
	void BindStage(FStage* InStage);
	void BindPauseFlag(bool* InPauseFlag);
	void SetStageChangeCallback(std::function<void(int)> Callback);
	void SetTotalStages(int Count);
	virtual void SetTextures(FGameContext& Context) override;

	void Update(FGameContext& Context) override;
	void Render(FGameContext& Context) override;

private:
	FStage* Stage = nullptr;
	bool*   PauseFlag = nullptr;
	bool    bDebugPaused = false;
	bool    bInvincible = false;
	bool    bTimeFrozen = false;
	bool    bDarknessDisabled = false;
	float   TimeScale = 1.0f;
	int     DarknessLevel = 2;
	int     TotalStages = 1;
	std::function<void(int)> OnStageChange;
};
