#pragma once

#include "Core/GameContext.h"
#include "UIWidget.h"
#include "Render/Texture.h"
#include "Core/Types.h"
#include "UI/widget/BeatEffect.h"
class FStage;

class FGameplayHUDWidget : public IUIWidget
{
public:
	void BindStage(const FStage* Stage);
	void BindPauseFlag(bool* InPauseFlag);

	void Update(FGameContext& Context) override;
	void Render(FGameContext& Context) override;
	virtual void SetTextures(FGameContext& Context) override;

	void ResetPlayTime();

private:
	const FStage* Stage = nullptr;
	bool* PauseFlag = nullptr;
	float         PlayTime = 0.0f;
	int           MaxHPToDisplay = 5;
	FTexture* LifeTexture = nullptr;
	FTexture* LifeDeadTexture = nullptr;
	FVec2 HPTextPos = { 700.0f, 20.0f };
	float HeartScale = 50.f;

	std::vector<BeatEffect> BeatEffects;
};
