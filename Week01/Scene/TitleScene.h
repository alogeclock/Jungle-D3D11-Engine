#pragma once

#include <memory>
#include "Scene.h"

class FPopupManager;

class FTitleScene : public IScene
{
public:
	~FTitleScene() override;

	void Enter() override;
	void Exit() override;

	void Update(FGameContext& Context) override;
	void Render(FGameContext& Context) override;

private:
	void StartGame();
	void ShowCredit();
	void ShowScore();

	std::unique_ptr<FPopupManager> PopupManager;
};
