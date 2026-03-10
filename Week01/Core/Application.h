#pragma once

#include <memory>
#include <optional>
#include <windows.h>
#include "GameContext.h"
#include "Input.h"
#include "Render/Renderer.h"
#include "Scene/SceneManager.h"
#include "Render/TextureManager.h"
#include "Render/FontManager.h"
#include "Time.h"
#include "FWindow.h"

class FApplication
{
public:
	bool Initialize(HINSTANCE hInstance, int ScreenWidth, int ScreenHeight);
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

	FWindow Window;

	std::unique_ptr<FTime>           Time;
	std::unique_ptr<FInput>          Input;
	std::unique_ptr<FRenderer>       Renderer;
	std::unique_ptr<FTextureManager> TextureManager;
	std::unique_ptr<FSceneManager>   SceneManager;
	std::unique_ptr<FFontManager>    FontManager;

	std::optional<FGameContext> GameContext;
};
