#include "Engine/Source/Runtime/Engine/Public/ImGuiManager.h"

void UImGuiManager::Create(HWND hWnd, URenderer* renderer)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplWin32_Init((void*)hWnd);
    ImGui_ImplDX11_Init(renderer->Device, renderer->DeviceContext);
}

void UImGuiManager::Update()
{
    beginFrame();

    // 로직
    ImGui::Begin("Test");

    endFrame();
}

void UImGuiManager::beginFrame()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame(); 
}

void UImGuiManager::endFrame()
{
    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void UImGuiManager::Release()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}
