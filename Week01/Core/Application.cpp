#include "pch.h"
#include <memory>
#include <string>
#include "Application.h"
#include "AudioSystem.h"
#include "Core/Input.h"
#include "Core/Time.h"
#include "Data/StageLoader.h"
#include "Data/CreditLoader.h"
#include "IO/BitmapFontLoader.h"
#include "IO/ImageLoader.h"
#include "Render/Renderer.h"
#include "Render/TextureManager.h"
#include "Scene/SceneManager.h"

bool FApplication::Initialize(HINSTANCE hInstance, int ScreenWidth, int ScreenHeight)
{
    if (!Window.Initialize(hInstance, ScreenWidth, ScreenHeight, L"My Game"))
        return false;

    HWND WindowHandle = Window.GetHandle();
    if (WindowHandle == nullptr)
        return false;

    Time = std::make_unique<FTime>();
    Input = std::make_unique<FInput>();
    Renderer = std::make_unique<FRenderer>();
    TextureManager = std::make_unique<FTextureManager>();
    SceneManager = std::make_unique<FSceneManager>();
    FontManager = std::make_unique<FFontManager>();

    if (!Renderer->Initialize(WindowHandle, ScreenWidth, ScreenHeight))
        return false;

    if (!InitializeResources())
        return false;

    if (!InitializeImGui(WindowHandle))
        return false;

    GameContext.emplace(FGameContext{*Time, *Input, *Renderer, *TextureManager, *FontManager});
    SceneManager->Initialize();

    return true;
}

bool FApplication::InitializeResources()
{
    if (!LoadDataResources())
        return false;

    if (!LoadFontResources())
        return false;

    if (!LoadSoundResources())
        return false;

    if (!LoadSpriteResources())
        return false;

    return true;
}

bool FApplication::LoadFontResources()
{
    auto FontTexture = FImageLoader::LoadAsTexture(Renderer->Device, std::string(BitmapFontPngPath));
    if (!FontTexture)
        return false;

    FontManager->Register("basic_font", std::string(BitmapFontFntPath), std::move(FontTexture));
    return true;
}

bool FApplication::LoadSoundResources()
{
    FAudioSystem::Get().Initialize();

    // 현재 구조상 사운드는 AudioSystem 초기화만 하고,
    // 개별 wav는 필요 시 재생 시점에 경로로 접근하는 방식이면 여기까지면 충분함.
    // 추후 프리로드 구조가 생기면 여기에서 한 번에 등록하면 됨.
    return true;
}

bool FApplication::LoadSpriteResources()
{
    static constexpr FTextureResourceDesc SpriteResources[] = {
        {"tile_floor", "tile_floor.png"},
        {"goal", "goal.png"},
        {"wall", "wall.png"},
        {"player_bunnie", "player_bunnie.png"},
        {"player_otaku", "player_otaku.png"},
        {"monster_stonegolem", "monster_stonegolem.png"},
        {"monster_firegolem", "monster_firegolem.png"},
        {"beat_bar", "beat_bar.png"},
        {"beat_heart", "beat_heart.png"},
        {"title_background", "title_background.png"},
        {"life_heart", "life_heart.png"},
        {"life_dead", "life_dead.png"},
        {"effect_perfect", "effect_perfect.png"},
        {"effect_great", "effect_great.png"},
        {"effect_good", "effect_good.png"},
        {"effect_miss", "effect_miss.png"},
        {"item_invincibility", "item_Invincibility.png"},
        {"item_time_scale_up", "item_time_scale_up.png"},
        {"item_time_scale_down", "item_time_scale_down.png"},
        {"item_darkness_up", "item_darkness_up.png"},
        {"item_darkness_down", "item_darkness_down.png"},
        {"item_time_freeze", "item_time_freeze.png"},
        {"compass", "compass.png"},
        {"compass_needle", "compass_needle.png"},
    };

    for (const FTextureResourceDesc &Resource : SpriteResources)
    {
        if (!LoadTextureResource(Resource.Key, std::string(SpriteDir) + Resource.FileName))
            return false;
    }

    return true;
}

bool FApplication::LoadTextureResource(const std::string &Key, const std::string &Path)
{
    if (TextureManager->Has(Key))
        return true;

    auto Texture = FImageLoader::LoadAsTexture(Renderer->Device, Path);
    if (!Texture)
        return false;

    TextureManager->Register(Key, std::move(Texture));
    return true;
}

bool FApplication::LoadDataResources()
{
    if (!FStageLoader::Get().Initialize(std::string(StageDataPath)))
        return false;

    if (!FCreditLoader::Get().Initialize(std::string(CreditDataPath)))
        return false;

    return true;
}

bool FApplication::InitializeImGui(HWND WindowHandle)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &Io = ImGui::GetIO();
    ImGui::StyleColorsDark();

    ImFont *KoreanFont = Io.Fonts->AddFontFromFileTTF(
        std::string(ImGuiFontPath).c_str(),
        18.0f,
        nullptr,
        Io.Fonts->GetGlyphRangesKorean());

    if (KoreanFont != nullptr)
    {
        Io.FontDefault = KoreanFont;
    }

    if (!ImGui_ImplWin32_Init(WindowHandle))
        return false;

    if (!ImGui_ImplDX11_Init(Renderer->Device, Renderer->DeviceContext))
        return false;

    return true;
}

/*
        1. Input 처리
        2. Time 처리 (실제 흐른 시각 기준)
        3. Scene 업데이트: Scene 타입을 구분하여 처리
        4. Scene 렌더
*/
void FApplication::Run()
{
    while (bIsRunning && Window.ProcessMessages())
    {
        Input->Update();
        Time->Update();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        SceneManager->Update(*GameContext);

        Renderer->BeginFrame();
        SceneManager->Render(*GameContext);
        ImGui::Render();
        Renderer->Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        Renderer->EndFrame();
    }
}

void FApplication::Shutdown()
{
    FAudioSystem::Get().Shutdown();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    Window.Shutdown();
}

void FApplication::RequestQuit() { bIsRunning = false; }

bool FApplication::IsRunning() const { return bIsRunning; }

FTime &FApplication::GetTime() { return *Time; }

FInput &FApplication::GetInput() { return *Input; }

FRenderer &FApplication::GetRenderer() { return *Renderer; }

FTextureManager &FApplication::GetTextureManager() { return *TextureManager; }

FSceneManager &FApplication::GetSceneManager() { return *SceneManager; }
