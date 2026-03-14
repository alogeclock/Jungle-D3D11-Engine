#include "Engine/Source/Runtime/Engine/Public/ImGuiManager.h"

ExampleAppConsole* GConsole = nullptr;

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
    
    // Control Panel
    ImGui::Begin("Jungle Control Panel");
    bool open = true;
    ShowExampleAppConsole(&open);

    ImGui::End();

    // Property Window
    ImGui::Begin("Jungle Property Window");

    if (SelectedObject != nullptr)
    {
        FTransform t = SelectedObject->GetTransform();

        ImGui::DragFloat3("Translation", &t.Location.X, 0.01f, -1.0f, 1.0f);
        ImGui::DragFloat3("Rotation", &t.Rotation.X, 0.01f, -5.0f, 5.0f);
        ImGui::DragFloat3("Scale", &t.Scale.X, 0.01f,-1.0f, 1.0f);

        SelectedObject->SetTransform(t);
    }

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

void UImGuiManager::SetSelectedObject(USphereComponent* sphere)
{
    SelectedObject = sphere;
}

void UImGuiManager::AddLog(char* msg)
{
    GConsole->AddLog(msg);
}
