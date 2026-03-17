#include "CoreTypes.h"
#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Engine/Public/ImGuiManager.h"

#include "World.h"
#include "Object/Actor.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/CubeComponent.h"

ExampleAppConsole *GConsole = nullptr;

void UImGuiManager::Create(HWND hWnd, URenderer *renderer)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    ImGui_ImplWin32_Init((void *)hWnd);
    ImGui_ImplDX11_Init(renderer->Device, renderer->DeviceContext);
}

void UImGuiManager::Update(URenderer *renderer)
{
    beginFrame();

    // Control Panel
    ImGui::Begin("Jungle Control Panel");
    ShowControlPanel();
    SpawnActors();
    ImGui::End();

    // Property Window
    ImGui::Begin("Jungle Property Window");
    TransformInspector();
    ImGui::End();

    // 임시
    ImGui::Begin("Test");

    if (ImGui::Button("Spawn Actors"))
    {


        AActor *NewActor = GWorld->SpawnActor<AActor>();

        USceneComponent *Root = NewActor->CreateDefaultSubobject<USceneComponent>();

        NewActor->SetRootComponent(Root);
        Root->RegisterComponent();

        UCubeComponent *Cube = NewActor->CreateDefaultSubobject<UCubeComponent>();
        Cube->RegisterComponent();

        NewActor->SetTransform(FTransform(FVector<float>(3.0f, 3.0f, 3.0f), FVector<float>(3.0f, 3.0f, 3.0f), FVector<float>(3.0f, 3.0f, 3.0f)));

        char logBuffer[256];

        // snprintf를 사용해 문장과 액터의 개수(%zu)를 버퍼에 합칩니다.
        snprintf(logBuffer, sizeof(logBuffer), "%d", static_cast<int>(GWorld->CurrentLevel->GetActors().size()));

        // 질문자님께서 만드신 AddLog 함수에 완성된 문자열 버퍼를 넘겨줍니다!
        AddLog(logBuffer);

        UClass *ClassToSpawn = UCubeComponent::StaticClass();
        UObject *NewObj = FObjectFactory::ConstructObject(ClassToSpawn);
        UCubeComponent *MyCube = Cast<UCubeComponent>(NewObj);

        if (MyCube)
        {

        }
    }

    // Console
    ImGui::Begin("Console");
    bool open = true;
    ShowExampleAppConsole(&open);
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

void UImGuiManager::SetSelectedObject(UPrimitiveComponent *primitiveComponent) { SelectedObject = primitiveComponent; }

bool UImGuiManager::IsCaptureMouse() { return ImGui::GetIO().WantCaptureMouse; }

void UImGuiManager::AddLog(const char *msg) { GConsole->AddLog(msg); }

void UImGuiManager::ShowControlPanel()
{
    ImGui::TextWrapped("FPS: %.f\n", UTimeManager::Get().GetFPS());
    ImGui::TextWrapped("FrameTime: %.1f (ms)\n", UTimeManager::Get().GetFrameTime());
    ImGui::Checkbox("Orthogonal", &UImGuiManager::Get().bIsOrthogonal);
}

void UImGuiManager::SpawnActors()
{
    const char *PrimitiveTypeStrings[] = {"Sphere", "Cube", "Triangle", "Plane", "Torus", "Arrow", "CubeArrow", "Ring", "Axis"};

    static int Primitive = 0;

    ImGui::Combo("Primitive", &Primitive, PrimitiveTypeStrings, IM_ARRAYSIZE(PrimitiveTypeStrings));

    if (ImGui::Button("Spawn Actors"))
    {
        if (Primitive == 0)
            return;

        AActor *NewActor = GWorld->SpawnActor<AActor>();

        USceneComponent *Root = NewActor->CreateDefaultSubobject<USceneComponent>();

        NewActor->SetRootComponent(Root);
        Root->RegisterComponent();

        UCubeComponent *Cube = NewActor->CreateDefaultSubobject<UCubeComponent>();
        Cube->RegisterComponent();

        NewActor->SetTransform(FTransform(FVector<float>(3.0f, 3.0f, 3.0f), FVector<float>(3.0f, 3.0f, 3.0f), FVector<float>(1.0f, 1.0f, 1.0f)));

        char logBuffer[256];
        // snprintf를 사용해 문장과 액터의 개수(%zu)를 버퍼에 합칩니다.
        snprintf(logBuffer, sizeof(logBuffer), "%d", static_cast<int>(GWorld->CurrentLevel->GetActors().size()));

        // 질문자님께서 만드신 AddLog 함수에 완성된 문자열 버퍼를 넘겨줍니다!
        AddLog(logBuffer);

        UClass         *ClassToSpawn = UCubeComponent::StaticClass();
        UObject        *NewObj = FObjectFactory::ConstructObject(ClassToSpawn);
        UCubeComponent *MyCube = Cast<UCubeComponent>(NewObj);
    }

    if (ImGui::Button("Save Scene"))
    {
        if (GWorld != nullptr)
        {
            // 기본 파일명 지정 (필요 시 ImGui::InputText로 파일명을 입력받도록 확장 가능)
            if (GWorld->SaveLevel("Data/SavedScene.Scene"))
            {
                AddLog("[System] Level saved successfully to 'Data/SavedScene.Scene'.");
            }
            else
            {
                AddLog("[Error] Failed to save level.");
            }
        }
    }

    if (ImGui::Button("Load Scene"))
    {
        if (GWorld != nullptr)
        {
            if (GWorld->LoadLevel("Data/SavedScene.Scene"))
            {
                AddLog("[System] Level loaded successfully from 'Data/SavedScene.Scene'.");
                SelectedObject = nullptr;
            }
            else
            {
                AddLog("[Error] Failed to load level.");
            }
        }
        SelectedObject = nullptr;
    }
}

void UImGuiManager::TransformInspector()
{
    if (SelectedObject != nullptr)
    {
        FTransform t = SelectedObject->GetTransform();

        ImGui::DragFloat3("Translation", &t.Location.X, 0.01f, -1.0f, 1.0f);
        ImGui::DragFloat3("Rotation", &t.Rotation.X, 0.01f, -5.0f, 5.0f);
        ImGui::DragFloat3("Scale", &t.Scale.X, 0.01f, -1.0f, 1.0f);

        SelectedObject->SetTransform(t);
    }
}