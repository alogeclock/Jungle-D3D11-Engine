#pragma once

#include <memory>
#include <optional>
#include "Core/GameContext.h"
#include "Scene/SceneManager.h"
#include "FWindow.h"

class FTime;
class FInput;
class FRenderer;
class FTextureManager;
class FFontManager;

class FApplication
{
  public:
    bool Initialize(HINSTANCE hInstance, int ScreenWidth, int ScreenHeight);
    void Run();
    void Shutdown();

    void RequestQuit();
    bool IsRunning() const;

    FTime &GetTime();
    FInput &GetInput();
    FRenderer &GetRenderer();
    FTextureManager &GetTextureManager();
    FSceneManager &GetSceneManager();

  private:
    bool InitializeResources();
    bool InitializeImGui(HWND WindowHandle);

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
