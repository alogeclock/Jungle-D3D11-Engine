#pragma once

#include <memory>
#include <string>
#include "Scene.h"

class FStage;
class FPopupManager;

class FPlayScene : public IScene
{
public:
	~FPlayScene() override;

	void Enter() override;
	void Exit() override;

	void Update(FGameContext& Context) override;
	void Render(FGameContext& Context) override;

	void StartNewGame(const std::string& MapPath);
	void RestartGame();

private:
	std::unique_ptr<FStage> Stage;
	std::unique_ptr<FPopupManager> PopupManager;

	bool bIsPaused = false;
	bool bIsGameOverPopupOpened = false;

	std::string CurrentMapPath;
};
