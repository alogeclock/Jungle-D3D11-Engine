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
        snprintf(logBuffer, sizeof(logBuffer), "%d", static_cast<int>(GWorld->GetCurrentLevel()->GetActors().size()));

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
    ImGui::TextWrapped("FPS: %.f \t FrameTime: %.1f (ms)\n", UTimeManager::Get().GetFPS(), UTimeManager::Get().GetFrameTime());

    ImGui::Separator();

    SpawnActors();
    NewScene();
    SaveScene();
    LoadScene();

    ImGui::Separator();

    ImGui::Checkbox("Orthogonal", &UImGuiManager::Get().bIsOrthogonal);
}

void UImGuiManager::SpawnActors()
{
    const char *PrimitiveTypeStrings[] = {"Sphere", "Cube", "Triangle"};

    static int Primitive = 0;
    static int NumberOfSpawn = 1;

    bool isSpawn = false;

    ImGui::Combo("Primitive", &Primitive, PrimitiveTypeStrings, IM_ARRAYSIZE(PrimitiveTypeStrings));

    isSpawn = ImGui::Button("Spawn");
    ImGui::SameLine();
    ImGui::DragInt("Number of spawn", &NumberOfSpawn, 0.05f, 1, 10);

    if (isSpawn)
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

        for (int i = 0; i < NumberOfSpawn; i++)
        {
            AActor          *NewActor = GWorld->SpawnActor<AActor>();
            USceneComponent *Root = NewActor->CreateDefaultSubobject<USceneComponent>();
            NewActor->SetRootComponent(Root);
            Root->RegisterComponent();

            UObject             *NewObj = FObjectFactory::ConstructObject(ComponentClassToSpawn);
            UPrimitiveComponent *DynamicPrimitive = Cast<UPrimitiveComponent>(NewObj); // 생성된 객체가 화면에 그릴 수 있는 PrimitiveComponent인지 확인

            if (DynamicPrimitive != nullptr)
            {
                const char *SpawnedClassName = DynamicPrimitive->GetClass()->GetName();

                // ⭐️ 2. 가져온 이름을 로그 버퍼에 예쁘게 포맷팅합니다.
                char logBuffer[256];
                snprintf(logBuffer, sizeof(logBuffer), "[System] Spawned Actor: %s", SpawnedClassName);

                // ⭐️ 3. 완성된 문자열을 로그로 출력합니다!
                AddLog(logBuffer);

                // 템플릿 CreateDefaultSubobject()가 내부적으로 해주던 작업을 직접 해줍니다.
                DynamicPrimitive->SetOuter(NewActor);  // 이 컴포넌트의 주인(AActor) 설정
                DynamicPrimitive->RegisterComponent(); // 시스템에 등록
            }
        }
    }
}

void UImGuiManager::NewScene()
{
    if (ImGui::Button("New Scene"))
    {
        if (GWorld && GWorld->GetCurrentLevel())
        {
            GWorld->GetCurrentLevel()->ClearActors();

            AddLog("[System] All actors and components have been destroyed.");
        }

        SelectedObject = nullptr;
    }
}

void UImGuiManager::SaveScene()
{
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

        SelectedObject = nullptr;
    }
}

void UImGuiManager::LoadScene()
{
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

    ImGui::DragFloat3("Translation", &t.Location.X, 0.01f);
    ImGui::DragFloat3("Rotation", &t.Rotation.X, 0.01f);
    ImGui::DragFloat3("Scale", &t.Scale.X, 0.01f);

    Actor->SetTransform(t);
}