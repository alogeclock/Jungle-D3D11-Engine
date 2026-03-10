#pragma once

#include <memory>
#include <optional>
#include "GameContext.h"
#include "Input.h"
#include "Renderer.h"
#include "Scene/SceneManager.h"
#include "TextureManager.h"
#include "Time.h"

struct FGameContext;

class FApplication
{
public:
	bool Initialize(HWND hWindow, int ScreenWidth, int ScreenHeight);
	void Run();
	void Shutdown();

	void RequestQuit();
	bool IsRunning() const;

	FTime& GetTime();
	FInput& GetInput();
	FRenderer& GetRenderer();
	FTextureManager& GetTextureManager();
	FSceneManager& GetSceneManager();

private:
	bool bIsRunning = true;
	HWND HWnd = nullptr;

	std::unique_ptr<FTime>           Time;
	std::unique_ptr<FInput>          Input;
	std::unique_ptr<FRenderer>       Renderer;
	std::unique_ptr<FTextureManager> TextureManager;
	std::unique_ptr<FSceneManager>   SceneManager;
	std::optional<FGameContext>      GameContext;

private:
	bool ProcessWindowMessages();
};
