#include "CoreTypes.h"
#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Engine/Public/ImGuiManager.h"

#include "World.h"
#include "Object/Actor.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/CubeComponent.h"

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

    ImGui::End();

    // 임시
    ImGui::Begin("Test");

    if (ImGui::Button("Spawn Actor"))
    {
        AActor *NewActor = GWorld->SpawnActor<AActor>();

        USceneComponent *Root = NewActor->CreateDefaultSubobject<USceneComponent>();

        NewActor->SetRootComponent(Root);
        Root->RegisterComponent();

        UCubeComponent *Cube = NewActor->CreateDefaultSubobject<UCubeComponent>();

        //for (auto& components : NewActor->GetRootComponent)
        //{
        //}
        //NewActor->OwnedComponents.

        char logBuffer[256];

        // snprintf를 사용해 문장과 액터의 개수(%zu)를 버퍼에 합칩니다.
        snprintf(logBuffer, sizeof(logBuffer), "%d", static_cast<int>(GWorld->CurrentLevel->Actors.size()));

        // 질문자님께서 만드신 AddLog 함수에 완성된 문자열 버퍼를 넘겨줍니다!
        AddLog(logBuffer);
    }

    ImGui::End();

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
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void UImGuiManager::Release()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void UImGuiManager::SetSelectedObject(UPrimitiveComponent* primitiveComponent)
{ 
    SelectedObject = primitiveComponent; 
}

void UImGuiManager::AddLog(char* msg)
{
    GConsole->AddLog(msg);
}

