#pragma once

#include <memory>
#include <string>
#include "Scene.h"
#include "UI/UIManager.h"

class FStage;
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

    void Update(FGameContext &Context) override;
    void Render(FGameContext &Context) override;

    void StartNewGame(const std::string &MapPath, int StageIndex = 0);
    void RestartGame();

private:
	std::unique_ptr<FStage> Stage;
	FUIManager UIManager;

	FRenderer* Renderer = nullptr;
	FTextureManager* Textures = nullptr;

    bool bIsPaused = false;
    bool bIsGameOverPopupOpened = false;

    std::string CurrentMapPath;
    int CurrentStageIndex = 0;
};
