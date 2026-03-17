#include "CoreTypes.h"
#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Engine/Public/ImGuiManager.h"

#include "World.h"
#include "Object/Actor.h"

#include "Engine/Source/Runtime/Engine/Public/Classes/Components/ArrowComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/AxisComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/CubeArrowComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/CubeComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/PlaneComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/RingComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/SphereComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/TorusComponent.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/TriangleComponent.h"

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
    ImGui::End();

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
    const char *PrimitiveTypeStrings[] = {"Sphere", "Cube", "Triangle"};

    static int Primitive = 0;

    ImGui::Combo("Primitive", &Primitive, PrimitiveTypeStrings, IM_ARRAYSIZE(PrimitiveTypeStrings));

    if (ImGui::Button("Spawn Actors"))
    {
        UClass *ComponentClassToSpawn = nullptr;

        switch (Primitive)
        {
        case 0:
            ComponentClassToSpawn = USphereComponent::StaticClass();
            break;
        case 1:
            ComponentClassToSpawn = UCubeComponent::StaticClass();
            break;
        case 2:
            ComponentClassToSpawn = UTriangleComponent::StaticClass();
            break;
        }

        if (ComponentClassToSpawn == nullptr)
        {
            AddLog("[Error] Failed to spawn: Invalid primitive type selected.");
        }

        AActor *NewActor = GWorld->SpawnActor<AActor>();
        USceneComponent *Root = NewActor->CreateDefaultSubobject<USceneComponent>();
        NewActor->SetRootComponent(Root);
        Root->RegisterComponent();

        UObject        *NewObj = FObjectFactory::ConstructObject(ComponentClassToSpawn);
        UPrimitiveComponent *DynamicPrimitive = Cast<UPrimitiveComponent>(NewObj); // 생성된 객체가 화면에 그릴 수 있는 PrimitiveComponent인지 확인

        if (DynamicPrimitive != nullptr)
        {
            AddLog("[System] ");

            // 템플릿 CreateDefaultSubobject()가 내부적으로 해주던 작업을 직접 해줍니다.
            DynamicPrimitive->SetOuter(NewActor);  // 이 컴포넌트의 주인(AActor) 설정
            DynamicPrimitive->RegisterComponent(); // 시스템에 등록
        }

        NewActor->SetTransform(FTransform(FVector<float>(0.0f, 0.0f, 0.0f), FVector<float>(0.0f, 0.0f, 0.0f), FVector<float>(1.0f, 1.0f, 1.0f)));
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
    if (SelectedObject == nullptr)
        return;
    
    AActor* Actor = Cast<AActor>(SelectedObject->GetOwner());
    FTransform t = Actor->GetTransform();

    ImGui::DragFloat3("Translation", &t.Location.X, 0.01f, -1.0f, 1.0f);
    ImGui::DragFloat3("Rotation", &t.Rotation.X, 0.01f, -5.0f, 5.0f);
    ImGui::DragFloat3("Scale", &t.Scale.X, 0.01f, -1.0f, 1.0f);

    Actor->SetTransform(t);
}