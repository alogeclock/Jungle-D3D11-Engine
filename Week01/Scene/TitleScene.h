#pragma once

#include "Scene.h"

class FPopupManager;

class FTitleScene : public IScene
{
public:
	void Enter() override;
	void Exit() override;

	void Update(FGameContext& Context) override;
	void Render(FGameContext& Context) override;

private:
	FPopupManager* PopupManager = nullptr;
};
