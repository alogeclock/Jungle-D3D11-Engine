#pragma once

#include <string>
#include "Scene.h"

class FStage;
class FPopupManager;

class FPlayScene : public IScene
{
public:
	void Enter() override;
	void Exit() override;

	void Update(FGameContext& Context) override;
	void Render(FGameContext& Context) override;

	void StartNewGame(const std::string& MapPath);
	void RestartGame();

private:
	FStage* Stage = nullptr;
	FPopupManager* PopupManager = nullptr;

	bool bIsPaused = false;
	bool bIsGameOverPopupOpened = false;

	std::string CurrentMapPath;
};
