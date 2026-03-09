#pragma once

#include <memory>

class FTime;
class FInput;
class FRenderer;
class FTextureManager;
class FSceneManager;
struct FGameContext;

class FApplication
{
public:
	bool Initialize(void* WindowHandle, int ScreenWidth, int ScreenHeight);
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

	std::unique_ptr<FTime> Time;
	std::unique_ptr<FInput> Input;
	std::unique_ptr<FRenderer> Renderer;
	std::unique_ptr<FTextureManager> TextureManager;
	std::unique_ptr<FSceneManager> SceneManager;
	std::unique_ptr<FGameContext> GameContext;
};
