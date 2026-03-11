#include "pch.h"
#include <memory>
#include "Application.h"
#include "AudioSystem.h"
#include "Core/Input.h"
#include "Core/Time.h"
#include "Data/StageLoader.h"
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
    auto LoadTex = [&](const std::string &Key, const std::string &Path)
    {
        if (!TextureManager->Has(Key))
        {
            auto Tex = FImageLoader::LoadAsTexture(Renderer->Device, Path);
            if (Tex)
            {
                TextureManager->Register(Key, std::move(Tex));
            }
        }
    };

    LoadTex("tile_floor", "Resources/Sprites/tile_floor.png");
    LoadTex("goal", "Resources/Sprites/goal.png");
    LoadTex("wall", "Resources/Sprites/wall.png");
    LoadTex("player", "Resources/Sprites/player_bunnie.png");
    LoadTex("monster_stonegolem", "Resources/Sprites/monster_stonegolem.png");
    LoadTex("monster_firegolem", "Resources/Sprites/monster_firegolem.png");
    LoadTex("beat_bar", "Resources/Sprites/beat_bar.png");
    LoadTex("title_background", "Resources/Sprites/title_background.png");
    LoadTex("life_heart", "Resources/Sprites/life_heart.png");
    LoadTex("used_heart", "Resources/Sprites/used_heart.png");
    LoadTex("beat_heart", "Resources/Sprites/beat_heart.png");
    LoadTex("effect_perfect", "Resources/Sprites/effect_perfect.png");
    LoadTex("effect_great", "Resources/Sprites/effect_great.png");
    LoadTex("effect_good", "Resources/Sprites/effect_good.png");
    LoadTex("effect_miss", "Resources/Sprites/effect_miss.png");

    LoadTex("item_invincibility", "Resources/Sprites/item_invincibility.png");
    LoadTex("item_time_scale_up", "Resources/Sprites/item_time_scale_up.png");
    LoadTex("item_time_scale_down", "Resources/Sprites/item_time_scale_down.png");
    LoadTex("item_darkness_up", "Resources/Sprites/item_darkness_up.png");
    LoadTex("item_darkness_down", "Resources/Sprites/item_darkness_down.png");
    LoadTex("item_time_freeze", "Resources/Sprites/item_time_freeze.png");

    LoadTex("compass", "Resources/Sprites/compass.png");
    LoadTex("compass_needle", "Resources/Sprites/compass_needle.png");

    auto FontTexture = FImageLoader::LoadAsTexture(Renderer->Device, "Resources/Fonts/bmFont.png");
    if (!FontTexture)
        return false;

    FontManager->Register("basic_font", "Resources/Fonts/bmFont.fnt", std::move(FontTexture));

    FAudioSystem::Get().Initialize();

    if (!FStageLoader::Get().Initialize("Resources/Maps/stages.json"))
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
       "Resources/Fonts/Galmuri11.ttf",
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
