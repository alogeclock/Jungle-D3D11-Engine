#pragma once

#include <memory>
#include <optional>
#include <string_view>
#include "Core/GameContext.h"
#include "Scene/SceneManager.h"
#include "FWindow.h"

class FTime;
class FInput;
class FRenderer;
class FTextureManager;
class FFontManager;

struct FTextureResourceDesc
{
    const char *Key;
    const char *FileName;
};

class FApplication
{
  public:
    bool Initialize(HINSTANCE hInstance, int ScreenWidth, int ScreenHeight);
    void Run();
    void Shutdown();

    void RequestQuit();
    bool IsRunning() const;

    FTime           &GetTime();
    FInput          &GetInput();
    FRenderer       &GetRenderer();
    FTextureManager &GetTextureManager();
    FSceneManager   &GetSceneManager();

  private:
    bool InitializeResources();
    bool InitializeImGui(HWND WindowHandle);

    bool LoadDataResources();
    bool LoadFontResources();
    bool LoadSoundResources();
    bool LoadSpriteResources();

    bool LoadTextureResource(const std::string &Key, const std::string &Path);

  private:
    static constexpr std::wstring_view WindowTitle = L"My Game";

    static constexpr std::string_view ResourceRoot = "Resources/";
    static constexpr std::string_view DataDir = "Resources/Data/";
    static constexpr std::string_view FontDir = "Resources/Fonts/";
    static constexpr std::string_view SoundDir = "Resources/Sounds/";
    static constexpr std::string_view SpriteDir = "Resources/Sprites/";

    static constexpr std::string_view StageDataPath = "Resources/Data/stages.json";
    static constexpr std::string_view CreditDataPath = "Resources/Data/credits.json";
    static constexpr std::string_view ScoreboardPath = "Resources/Data/scoreboard.json";
    static constexpr std::string_view ImGuiFontPath = "Resources/Fonts/Galmuri11.ttf";
    static constexpr std::string_view BitmapFontFntPath = "Resources/Fonts/bmFont.fnt";
    static constexpr std::string_view BitmapFontPngPath = "Resources/Fonts/bmFont.png";

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
