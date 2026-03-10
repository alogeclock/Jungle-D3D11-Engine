#pragma once

#include <memory>
#include <string>
#include "Scene.h"
#include "UI/UIManager.h"

class FStage;
class FRenderer;
class FTextureManager;
class FFontManager;
class FPlayScene : public IScene
{
public:
	~FPlayScene() override;

	void SetRenderer(FRenderer* InRenderer);
	void SetTextureManager(FTextureManager* InTextures);
	void SetFontManager(FFontManager* FontManager);

	void Enter() override;
	void Exit() override;

	void Update(FGameContext& Context) override;
	void Render(FGameContext& Context) override;

	void StartNewGame(int StageIndex = 0);
	void RestartGame();

private:
	std::unique_ptr<FStage> Stage;
	FUIManager UIManager;

	FRenderer* Renderer = nullptr;
	FTextureManager* Textures = nullptr;
	FFontManager* FontManager = nullptr;

	bool bIsPaused = false;
	bool bIsGameOverPopupOpened = false;

	int CurrentStageIndex = 0;
};
