#pragma once

#include <memory>
#include <string>
#include "Scene.h"

class FStage;
class FPopupManager;
class FRenderer;
class FTextureManager;

class FPlayScene : public IScene
{
public:
	~FPlayScene() override;

	void SetRenderer(FRenderer* InRenderer);
	void SetTextureManager(FTextureManager* InTextures);

	void Enter() override;
	void Exit() override;

	void Update(FGameContext& Context) override;
	void Render(FGameContext& Context) override;

	void StartNewGame(const std::string& MapPath);
	void RestartGame();

private:
	std::unique_ptr<FStage> Stage;
	std::unique_ptr<FPopupManager> PopupManager;

	FRenderer* Renderer = nullptr;
	FTextureManager* Textures = nullptr;

	bool bIsPaused = false;
	bool bIsGameOverPopupOpened = false;

	std::string CurrentMapPath;
};
