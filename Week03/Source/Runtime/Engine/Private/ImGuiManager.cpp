#include "CoreTypes.h"
#include "Source/Runtime/Core/Public/Memory.h"
#include "Source/Runtime/Engine/Public/ImGuiManager.h"

#include "World.h"
#include "Source/Runtime/Engine/Object/Public/Actor.h"

#include "Source/Runtime/Engine/Public/Classes/Components/ArrowComponent.h"
#include "Source/Runtime/Engine/Public/Classes/Components/AxisComponent.h"
#include "Source/Runtime/Engine/Public/Classes/Components/CubeArrowComponent.h"
#include "Source/Runtime/Engine/Public/Classes/Components/CubeComponent.h"
#include "Source/Runtime/Engine/Public/Classes/Components/PlaneComponent.h"
#include "Source/Runtime/Engine/Public/Classes/Components/RingComponent.h"
#include "Source/Runtime/Engine/Public/Classes/Components/SphereComponent.h"
#include "Source/Runtime/Engine/Public/Classes/Components/TriangleComponent.h">

#include "Source/Runtime/Editor/Public/EditorViewportClient.h"

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

    // Console
    ImGui::Begin("Console");
    bool open = true;
    ShowExampleAppConsole(&open);
    ImGui::End();

    ImGui::Begin("Log");
    if (ImGui::Button("GUObjectArray", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
    {
        for (UObject* Object : GUObjectArray)
        {
            FString ObjectName = Object->GetName();
            AddLog("Object Name : " + ObjectName);
        }
    }

    if (ImGui::Button("Actors", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
    {
        for (AActor *Actor : GWorld->GetCurrentLevel()->GetActors())
        {
            FString ActorName = Actor->GetName();
            AddLog("Actor Name : " + ActorName);

            for (UActorComponent *Component : Actor->GetOwnedComponents())
            {
                FString ComponentName = Component->GetName();
                AddLog("    ComponentName Name : " + ComponentName);
            }
        }
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

void UImGuiManager::SetCamera(FViewportCameraTransform *camera) { Camera = camera; }

void UImGuiManager::SetSelectedObject(UPrimitiveComponent *primitiveComponent) { SelectedObject = primitiveComponent; }

bool UImGuiManager::IsCaptureMouse() { return ImGui::GetIO().WantCaptureMouse; }

bool UImGuiManager::IsCaptureKeyboard() {return ImGui::GetIO().WantCaptureKeyboard;}

char* UImGuiManager::FStringTochar(FString string)
{
    char TempBuffer[256];
    
    snprintf(TempBuffer, sizeof(TempBuffer), "%s", string.c_str());

    return TempBuffer;
}

void UImGuiManager::AddLog(const char *msg) { GConsole->AddLog(msg); }

void UImGuiManager::AddLog(const FString &msg) { GConsole->AddLog("%s", msg.c_str()); }

void UImGuiManager::ShowControlPanel()
{
    if (buffer[0] == '\0' && GWorld && GWorld->GetCurrentLevel())
    {
        snprintf(buffer, sizeof(buffer), "%s", GWorld->GetCurrentLevel()->GetName().c_str());
    }

    ImGui::TextWrapped("FPS: %.f \t FrameTime: %.1f (ms)\n", UTimeManager::Get().GetFPS(), UTimeManager::Get().GetFrameTime());
    ImGui::TextWrapped("Allocated Bytes: %u", TotalAllocationBytes);
    ImGui::TextWrapped("Allocated Count: %u", TotalAllocationCount);
    ImGui::TextWrapped("Object Count: %u", GUObjectArray.size());

    ImGui::Separator();

    SpawnActors();

    ImGui::Separator();

    ImGui::InputText("Scene Name", buffer, IM_ARRAYSIZE(buffer));

    NewScene();
    SaveScene();
    LoadScene();

    ImGui::Separator();

    SetCameraInfo();
}

void UImGuiManager::SpawnActors()
{
    const char *PrimitiveTypeStrings[] = {"Sphere", "Cube", "Triangle", "Plane"};

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
        case 3:
            ComponentClassToSpawn = UPlaneComponent::StaticClass();
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

            UObject *NewObj = FObjectFactory::ConstructObject(ComponentClassToSpawn);
            UPrimitiveComponent *DynamicPrimitive = Cast<UPrimitiveComponent>(NewObj); // 생성된 객체가 화면에 그릴 수 있는 PrimitiveComponent인지 확인

            if (DynamicPrimitive != nullptr)
            {
                const char *SpawnedClassName = DynamicPrimitive->GetClass()->GetName();
                const uint32 UUID = NewObj->GetUUID();

                // ?? 2. 가져온 이름을 로그 버퍼에 예쁘게 포맷팅합니다.
                char logBuffer[256];
                snprintf(logBuffer, sizeof(logBuffer), "[System] Spawned Actor: %s / UUID: %u", SpawnedClassName, UUID);

                // ?? 3. 완성된 문자열을 로그로 출력합니다!
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
    if (ImGui::Button("New Scene", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
    {
        if (GWorld && GWorld->GetCurrentLevel())
        {
            GWorld->GetCurrentLevel()->ClearActors();
            GWorld->GetCurrentLevel()->SetName("DefaultLevel");
            snprintf(buffer, sizeof(buffer), "%s", GWorld->GetCurrentLevel()->GetName().c_str());
            AddLog("[System] All actors and components have been destroyed.");
        }

        SelectedObject = nullptr;
    }
}

void UImGuiManager::SaveScene()
{
    if (ImGui::Button("Save Scene", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
    {
        FString SceneName(buffer);

        FString DirectoryPath = "Data";

        // Data 폴더가 존재하지 않으면 생성
        if (!std::filesystem::exists(DirectoryPath))
        {
            std::filesystem::create_directory(DirectoryPath);
        }

        FString FilePath = DirectoryPath + "/" + SceneName + ".Scene";

        bool bSuccess = GWorld->SaveLevel(FilePath);

        if (bSuccess)
        {
            //GWorld->CurrentSceneName = SceneName;
            AddLog("Scene saved successfully: " + FilePath);
        }
        else
        {
            AddLog("Failed to save scene: " + FilePath);
        }
    }
}

void UImGuiManager::LoadScene()
{
    if (ImGui::Button("Load Scene", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
    {
        FString FilePath = OpenFileDialog();
        if (!FilePath.empty())
        {
            GWorld->LoadLevel(FilePath);

            // 불러온 파일 경로에서 확장자를 제외한 파일명만 추출 (예: "Data/MyScene.Scene" -> "MyScene")
            std::filesystem::path path(FilePath);
            FString               LoadedSceneName = path.stem().string();

            // GWorld의 씬 이름과 ImGui UI의 buffer를 갱신
            //GWorld->CurrentSceneName = LoadedSceneName;
            strcpy_s(buffer, sizeof(buffer), LoadedSceneName.c_str());

            AddLog("Scene loaded successfully: " + FilePath);
        }

        SelectedObject = nullptr;
    }
}

void UImGuiManager::SetCameraInfo()
{
    if (Camera == nullptr)
        return;

    ImGui::Checkbox("Orthogonal", &UImGuiManager::Get().bIsOrthogonal);

    float fovDeg = Camera->GetFOV() * 180.0f / 3.14159265f;

    if (ImGui::DragFloat("FOV", &fovDeg, 0.1f, 0.0f, 180.0f))
    {
        Camera->SetFOV(fovDeg * 3.14159265f / 180.0f);
    }

    ImGui::DragFloat3("Camera Location", &Camera->GetLocation().X, 0.01f);
    ImGui::DragFloat3("Camera Rotation", &Camera->GetRotation().X, 0.1f);
}

void UImGuiManager::TransformInspector()
{
    if (SelectedObject == nullptr)
        return;

    AActor    *Actor = Cast<AActor>(SelectedObject->GetOwner());
    FTransform t = Actor->GetTransform();

    ImGui::DragFloat3("Translation", &t.Location.X, 0.01f);
    ImGui::DragFloat3("Rotation", &t.Rotation.X, 0.01f);
    ImGui::DragFloat3("Scale", &t.Scale.X, 0.01f, 0.f, FLT_MAX);

    if (ImGui::Button("Change Mode"))
        bChangeMode = true;

    Actor->SetTransform(t);
}

FString UImGuiManager::SaveFileDialog()
{
    OPENFILENAMEA ofn;
    CHAR          szFile[260] = {0};

    ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);

    ofn.lpstrFilter = "Scene Files (*.Scene)\0*.Scene\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;

    // OFN_OVERWRITEPROMPT: 이미 존재하는 파일 선택 시 덮어쓸지 묻는 경고창을 띄웁니다.
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    // 사용자가 확장자를 입력하지 않아도 자동으로 .Scene을 붙여줍니다.
    ofn.lpstrDefExt = "Scene";

    // 다이얼로그 호출 (Save)
    if (GetSaveFileNameA(&ofn) == TRUE)
    {
        return FString(ofn.lpstrFile);
    }

    return FString("");
}

FString UImGuiManager::OpenFileDialog()
{
    OPENFILENAMEA ofn;
    CHAR          szFile[260] = {0};

    ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = NULL;

    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);

    // 파일 필터 설정 (.Scene 파일 전용)
    ofn.lpstrFilter = "Scene Files (*.Scene)\0*.Scene\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;

    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    // 대화상자 호출 (불러오기)
    if (GetOpenFileNameA(&ofn) == TRUE)
    {
        return FString(ofn.lpstrFile);
    }

    return FString("");
}
