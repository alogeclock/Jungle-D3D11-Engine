#include "pch.h"
#include <memory>
#include "Application.h"
#include "AudioSystem.h"
#include "Core/Input.h"
#include "Core/Time.h"
#include "Data/StageLoader.h"
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

    if (!Renderer->Initialize(WindowHandle, ScreenWidth, ScreenHeight))
        return false;

    // TEST Register

    auto LoadTex = [&](const std::string &Key, const std::string &Path)
    {
        if (!TextureManager->Has(Key))
        {
            auto Tex = FImageLoader::LoadAsTexture(Renderer->Device, Path);
            if (Tex)
                TextureManager->Register(Key, std::move(Tex));
        }
    };
    LoadTex("tile_floor", "Resources/Sprites/tile_floor.png");
    LoadTex("goal", "Resources/Sprites/goal.png");
    LoadTex("wall", "Resources/Sprites/wall.png");
    LoadTex("player", "Resources/Sprites/player.png");
    LoadTex("monster", "Resources/Sprites/monster.png");
    LoadTex("beat_bar", "Resources/Sprites/beat_bar.png");
    LoadTex("title_background", "Resources/Sprites/title_background.png");
    LoadTex("test", "Resources/Sprites/test.png");

    // 오디오 초기화
    FAudioSystem::Get().Initialize();

    // 스테이지 데이터 로드
    FStageLoader::Get().Initialize("Resources/Maps/stages.json");

    // TODO
    // TextureManager->Initialize(Renderer->Device);

    // 스테이지 데이터 로드
    FStageLoader::Get().Initialize("Resources/Maps/stages.json");

    // ImGui 초기화
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(WindowHandle);
    ImGui_ImplDX11_Init(Renderer->Device, Renderer->DeviceContext);

    GameContext.emplace(FGameContext{*Time, *Input, *Renderer, *TextureManager});
    SceneManager->Initialize();

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
        // Input, Time 업데이트
        Input->Update();
        Time->Update();

        // 프레임 렌더 준비
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Scene 업데이트
        SceneManager->Update(*GameContext);

        // Scene 렌더
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
