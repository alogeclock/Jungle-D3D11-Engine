#include "Application.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

bool FApplication::Initialize(HWND WindowHandle, int ScreenWidth, int ScreenHeight)
{
    HWnd = WindowHandle;

    Time = std::make_unique<FTime>();
    Input = std::make_unique<FInput>();
    Renderer = std::make_unique<FRenderer>();
    TextureManager = std::make_unique<FTextureManager>();
    SceneManager = std::make_unique<FSceneManager>();

    if (!Renderer->Initialize(HWnd, ScreenWidth, ScreenHeight))
        return false;

    TextureManager->Initialize(Renderer->Device);

    GameContext.emplace(FGameContext{*Time, *Input, *Renderer, *TextureManager});

    SceneManager->Initialize(&GameContext.value());
    SceneManager->ChangeSceneImmediately(ESceneType::Title);

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
    while (bIsRunning)
    {
        if (!ProcessWindowMessages())
        {
            bIsRunning = false;
            break;
        }

        // Input, Time 업데이트
        Input->Update();
        Time->Update();

        // 프레임 렌더 준비
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Scene 업데이트
        SceneManager->Update();

        // Scene 렌더
        Renderer->BeginFrame();
        SceneManager->Render();
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        Renderer->EndFrame();
    }
}

void FApplication::Shutdown()
{
    // todo
}

void FApplication::RequestQuit() { bIsRunning = false; }

bool FApplication::IsRunning() const { return bIsRunning; }

FTime &FApplication::GetTime() { return *Time; }

FInput &FApplication::GetInput() { return *Input; }

FRenderer &FApplication::GetRenderer() { return *Renderer; }

FTextureManager &FApplication::GetTextureManager() { return *TextureManager; }

FSceneManager &FApplication::GetSceneManager() { return *SceneManager; }

// false 반환시 종료
bool FApplication::ProcessWindowMessages()
{
    MSG Msg{};

    while (PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);

        if (Msg.message == WM_QUIT)
        {
            return false;
        }
    }

    return true;
}