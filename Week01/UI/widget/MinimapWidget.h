#pragma once

#include "UIWidget.h"
#include "Core/GameContext.h"

class FStage;

class FMinimapWidget : public IUIWidget
{
public:
	void BindStage(const FStage* InStage);

	void Update(FGameContext& Context) override;
	void Render(FGameContext& Context) override;
	void SetTextures(FGameContext& Context) override {}

private:
	const FStage* Stage = nullptr;

	float ExitAngle = 0.0f;
	float ItemAngle = 0.0f;
	bool  bHasNearestItem = false;
};
