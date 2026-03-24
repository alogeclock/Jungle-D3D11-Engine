#include "Source/Engine/Public/GUI/ImGuiManager.h"
#include "CoreTypes.h"
#include "Source/Core/Public/Memory.h"

#include "Source/Engine/Object/Public/Actor.h"
#include "World.h"

#include "Source/Engine/Public/Classes/Components/ArrowComponent.h"
#include "Source/Engine/Public/Classes/Components/AxisComponent.h"
#include "Source/Engine/Public/Classes/Components/CubeArrowComponent.h"
#include "Source/Engine/Public/Classes/Components/CubeComponent.h"
#include "Source/Engine/Public/Classes/Components/ParticleSubUVComponent.h"
#include "Source/Engine/Public/Classes/Components/PlaneComponent.h"
#include "Source/Engine/Public/Classes/Components/RingComponent.h"
#include "Source/Engine/Public/Classes/Components/SphereComponent.h"
#include "Source/Engine/Public/Classes/Components/TextComponent.h"
#include "Source/Engine/Public/Classes/Components/TriangleComponent.h"
#include "Source/Engine/Public/Classes/Components/UUIDTextComponent.h"

#include "Source/Editor/Public/Axis.h"
#include "Source/Editor/Public/EditorViewportClient.h"
#include "Source/Editor/Public/Grid.h"
#include "Source/Engine/Public/Classes/Components/AxisComponent.h"
#include "Source/Engine/Public/Classes/Components/GridComponent.h"

#include "Source/Core/Public/FName.h"

#include <iostream>

ExampleAppConsole* GConsole = nullptr;

void UImGuiManager::Create(HWND hWnd, URenderer* renderer)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    std::filesystem::path FontFilePath = "Data/Font/NanumGothic.ttf";

    if (!std::filesystem::exists(FontFilePath))
    {
        MessageBoxW(nullptr, FontFilePath.c_str(), L"폰트 파일 없음", MB_OK);
    }
    io.Fonts->AddFontFromFileTTF((char*)FontFilePath.u8string().c_str(), 16.0f, nullptr,
                                 io.Fonts->GetGlyphRangesKorean());

    ImGui_ImplWin32_Init((void*)hWnd);
    ImGui_ImplDX11_Init(renderer->Device, renderer->DeviceContext);
}

void UImGuiManager::Update()
{
    beginFrame();
    ImGuiIO& io = ImGui::GetIO();

    float cellWidth = io.DisplaySize.x * 0.1f;
    float cellHeight = io.DisplaySize.y * 0.1f;

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - cellWidth * 2, 0));
    ImGui::SetNextWindowSize(ImVec2(cellWidth * 2, io.DisplaySize.y));
    ImGui::Begin("Outliner");
    FOutliner.ShowOutliner();
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - cellWidth * 4.f, 0));
    ImGui::SetNextWindowSize(ImVec2(cellWidth * 2, cellHeight));
    ImGui::Begin("EngineStatics");
    FEngineStaticsGUI.ShowPanel();
    ImGui::End();

    ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && (delta.x * delta.x + delta.y * delta.y) < 4.0f)
    {
        ImGui::SetNextWindowPos(io.MousePos);
        FSpawnGUI.SetVisible(true);
    }

    if (FSpawnGUI.GetVisible())
    {
        ImGui::Begin("Spawn", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);
        FSpawnGUI.ShowPanel();
        ImGui::End();
    }

    if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse)
    {
        FSpawnGUI.SetVisible(false);
    }


    // Control Panel
    ImGui::Begin("Jungle Control Panel");
    ShowControlPanel();
    ImGui::End();

    // Property Window
    // ImGui::Begin("Jungle Property Window");
    // TransformInspector();
    // ImGui::End();

    // Console
    ImGui::Begin("Console");
    bool open = true;
    ShowExampleAppConsole(&open);
    ImGui::End();

    // ImGui::Begin("Log");
    // if (ImGui::Button("GUObjectArray", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
    //{
    //     for (UObject* Object : GUObjectArray)
    //     {
    //         FName ObjectName = Object->GetName();
    //         FString msg = "Object Name : ";
    //         AddLog(msg + ObjectName);
    //     }
    // }

    // if (ImGui::Button("Actors", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
    //{
    //     for (AActor* Actor : GWorld->GetCurrentLevel()->GetActors())
    //     {
    //         FName ActorName = Actor->GetName();
    //         FString msg = "Actor Name : ";
    //         AddLog(msg + ActorName);

    //        for (UActorComponent* Component : Actor->GetOwnedComponents())
    //        {
    //            FName ComponentName = Component->GetName();
    //            FString msg = "    ComponentName Name : ";

    //            AddLog(msg + ComponentName);
    //        }
    //    }
    //}
    // ImGui::End();

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

bool UImGuiManager::IsCaptureMouse()
{
    return ImGui::GetIO().WantCaptureMouse;
}

bool UImGuiManager::IsCaptureKeyboard()
{
    return ImGui::GetIO().WantCaptureKeyboard;
}

char* UImGuiManager::FStringToChar(FString string)
{
    char TempBuffer[256];

    snprintf(TempBuffer, sizeof(TempBuffer), "%s", string.c_str());

    return TempBuffer;
}

FString UImGuiManager::WStringToFString(const std::wstring& string)
{
    if (string.empty())
        return std::string();

    int size_needed = WideCharToMultiByte(CP_ACP, 0, &string[0], (int)string.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_ACP, 0, &string[0], (int)string.size(), &strTo[0], size_needed, NULL, NULL);

    return strTo;
}

std::wstring UImGuiManager::CharToWString(const char* charStr)
{
    // 입력된 문자열이 비어있는 경우 빈 wstring 반환
    if (charStr == nullptr || charStr[0] == '\0')
        return std::wstring();

    // 1. 변환 후의 와이드 문자열 길이를 먼저 계산하고, 계산된 길이만큼 공간을 할당합니다.
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, charStr, -1, NULL, 0);
    std::wstring wstrTo(size_needed, 0);

    // 2. 실제 멀티바이트(char) 문자열을 와이드(wchar_t) 문자열로 변환합니다.
    MultiByteToWideChar(CP_UTF8, 0, charStr, -1, &wstrTo[0], size_needed);

    // 3. 변환 시 포함된 C 스타일 널 종료 문자('\0')를 std::wstring에서 제거합니다.
    if (!wstrTo.empty() && wstrTo.back() == L'\0')
    {
        wstrTo.pop_back();
    }

    return wstrTo;
}

void UImGuiManager::AddLog(const char* msg)
{
    GConsole->AddLog(msg);
}

void UImGuiManager::AddLog(const FString& msg)
{
    GConsole->AddLog("%s", msg.c_str());
}

void UImGuiManager::AddLog(const std::wstring& msg)
{
    if (msg.empty())
        return;

    // 1. UTF-16(wstring)을 UTF-8(string)로 변환하는 데 필요한 버퍼 크기 계산
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &msg[0], (int)msg.size(), NULL, 0, NULL, NULL);

    // 2. 변환된 UTF-8 문자열을 담을 string 할당 및 변환 수행
    std::string utf8_msg(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &msg[0], (int)msg.size(), &utf8_msg[0], size_needed, NULL, NULL);

    // 3. 기존 출력 로직을 사용하여 UTF-8 문자열 출력
    GConsole->AddLog("%s", utf8_msg.c_str());
}

void UImGuiManager::ShowControlPanel()
{
    //ImGui::Separator();

    //SpawnActors();

    ImGui::Separator();

    if (buffer[0] == '\0' && GWorld && GWorld->GetCurrentLevel())
    {
        snprintf(buffer, sizeof(buffer), "%s", GWorld->GetCurrentLevel()->GetName().ToString().c_str());
    }

    ImGui::InputText("Scene Name", buffer, IM_ARRAYSIZE(buffer));

    NewScene();
    SaveScene();
    LoadScene();

    ImGui::Separator();

    SetCameraInfo();

    ImGui::Separator();

    FEditorViewportClient* ViewportClient = GEditor->GetEditorViewportClient();
    if (ViewportClient != nullptr)
    {
        const char* ViewModeStrings[] = {"Lit", "Unlit", "Wireframe"};

        EViewModeIndex currentMode = ViewportClient->GetViewMode();
        int currentItem = static_cast<int>(currentMode);

        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.5f);
        ImGui::Text("View Mode");
        if (ImGui::ListBox("##View Mode", &currentItem, ViewModeStrings, IM_ARRAYSIZE(ViewModeStrings)))
        {
            ViewportClient->SetViewMode(static_cast<EViewModeIndex>(currentItem));
        }

        ImGui::Separator();
        ImGui::Text("Show Flags");

        bool bDrawAABB = ViewportClient->GetDrawAABB();
        if (ImGui::Checkbox("Show AABB", &bDrawAABB))
        {
            ViewportClient->SetDrawAABB(bDrawAABB);
        }

        // 현재 뷰포트 클라이언트의 ShowFlags 값을 가져옵니다.
        EEngineShowFlags CurrentFlags = ViewportClient->GetShowFlags();

        // 1. Primitive 표시 여부 체크박스
        bool bShowPrimitives = (CurrentFlags & EEngineShowFlags::SF_Primitives) != EEngineShowFlags::None;
        if (ImGui::Checkbox("Show Primitives", &bShowPrimitives))
        {
            if (bShowPrimitives)
                CurrentFlags |= EEngineShowFlags::SF_Primitives; // 비트 켜기 (OR)
            else
                CurrentFlags &= ~EEngineShowFlags::SF_Primitives; // 비트 끄기 (AND NOT)
        }

        bool bShowUUID = (CurrentFlags & EEngineShowFlags::SF_UUID) != EEngineShowFlags::None;
        if (ImGui::Checkbox("Show UUID", &bShowUUID))
        {
            if (bShowUUID)
                CurrentFlags |= EEngineShowFlags::SF_UUID; // 비트 켜기 (OR)
            else
                CurrentFlags &= ~EEngineShowFlags::SF_UUID; // 비트 끄기 (AND NOT)
        }

        // 변경된 상태를 다시 뷰포트 클라이언트에 저장
        ViewportClient->SetShowFlags(CurrentFlags);
    }
}

void UImGuiManager::SpawnActors()
{
    const char* PrimitiveTypeStrings[] = {"Sphere", "Cube", "Triangle", "Plane", "Text", "UI", "Smoke"};

    static int Primitive = 0;
    static int NumberOfSpawn = 1;

    bool isSpawn = false;

    ImGui::Combo("Primitive", &Primitive, PrimitiveTypeStrings, IM_ARRAYSIZE(PrimitiveTypeStrings));

    isSpawn = ImGui::Button("Spawn");
    ImGui::SameLine();

    ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.3f);
    ImGui::DragInt("Number of spawn", &NumberOfSpawn, 0.05f, 1, 10);

    if (isSpawn)
    {
        UClass* ComponentClassToSpawn = nullptr;

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
        case 4:
            ComponentClassToSpawn = UTextComponent::StaticClass();
            break;
        case 5:
            ComponentClassToSpawn = UUUIDTextComponent::StaticClass();
            break;
        case 6:
            ComponentClassToSpawn = UParticleSubUVComponent::StaticClass();
            break;
        }

        if (ComponentClassToSpawn == nullptr)
        {
            AddLog("[Error] Failed to spawn: Invalid primitive type selected.");
        }

        for (int i = 0; i < NumberOfSpawn; i++)
        {
            AActor* NewActor = GWorld->SpawnActor<AActor>();
            USceneComponent* Root = NewActor->CreateDefaultSubobject<USceneComponent>();
            NewActor->SetRootComponent(Root);
            Root->RegisterComponent();

            UObject* NewComponent = FObjectFactory::ConstructObject(ComponentClassToSpawn);

            // 생성된 객체가 화면에 그릴 수 있는 PrimitiveComponent인지 확인
            UPrimitiveComponent* DynamicPrimitive = Cast<UPrimitiveComponent>(NewComponent);
            if (DynamicPrimitive != nullptr)
            {
                const char* SpawnedClassName = DynamicPrimitive->GetClass()->GetName();
                const uint32 UUID = NewActor->GetUUID();

                char logBuffer[256];
                snprintf(logBuffer, sizeof(logBuffer), "[System] Spawned Actor: %s / UUID: %u", SpawnedClassName, UUID);

                AddLog(logBuffer);

                DynamicPrimitive->SetOuter(NewActor);
                DynamicPrimitive->RegisterComponent();

                if (UTextComponent* TextComp = Cast<UTextComponent>(DynamicPrimitive))
                {
                    if (TextComp->IsExactly(UTextComponent::StaticClass()))
                        TextComp->SetText(FString(reinterpret_cast<const char*>(u8"박상혁 김호준 전현길 김기홍")));
                }

                UObject* NewUUUIDComponent = FObjectFactory::ConstructObject(UUUIDTextComponent::StaticClass());
                UUUIDTextComponent* UUUID = Cast<UUUIDTextComponent>(NewUUUIDComponent);
                if (UUUID == nullptr)
                {
                    return;
                }

                UUUID->SetOuter(NewActor);
                UUUID->RegisterComponent();
                UUUID->SetText(UUID);
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
            ULevel* OldLevel = GWorld->GetCurrentLevel();
            ULevel* NewLevel = GWorld->CreateNewLevel("DefaultLevel");
            GWorld->SetCurrentLevel(NewLevel);

            // 선택된 객체 초기화
            if (GEditor && GEditor->GetSelection())
            {
                GEditor->GetSelection()->Clear();
                GEditor->GetInputListeners()->clear();
            }

            if (OldLevel != nullptr)
            {
                GWorld->GetLevels().erase(OldLevel); // UWorld에 GetLevels() 필요
                delete OldLevel;
            }

            snprintf(buffer, sizeof(buffer), "%s", GWorld->GetCurrentLevel()->GetName().ToString().c_str());
            AddLog("[System] All actors and components have been destroyed.");

            if (GApplication)
                GApplication->UpdateEditorViewport();
        }
    }
}

void UImGuiManager::SaveScene()
{
    if (ImGui::Button("Save Scene", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
    {
        std::wstring SceneName(CharToWString(buffer));

        std::wstring DirectoryPath = L"Data/Scene";

        // Data 폴더가 존재하지 않으면 생성
        if (!std::filesystem::exists(DirectoryPath))
        {
            std::filesystem::create_directory(DirectoryPath);
        }

        std::wstring FilePath = DirectoryPath + L"/" + SceneName + L".Scene";

        bool bSuccess = GWorld->SaveLevel(FilePath);

        if (bSuccess)
        {
            // GWorld->CurrentSceneName = SceneName;
            AddLog(L"Scene saved successfully: " + FilePath);
        }
        else
        {
            AddLog(L"Failed to save scene: " + FilePath);
        }
    }
}

void UImGuiManager::LoadScene()
{
    if (ImGui::Button("Load Scene", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
    {
        std::wstring FilePath = OpenFileDialog();

        // 선택된 객체 초기화
        if (GEditor && GEditor->GetSelection())
        {
            GEditor->GetSelection()->Clear();
            GEditor->GetInputListeners()->clear();
        }

        if (!FilePath.empty() && GWorld->LoadLevel(FilePath))
        {
            // 불러온 파일 경로에서 확장자를 제외한 파일명만 추출
            std::filesystem::path path(FilePath);
            std::wstring wStem = path.stem().wstring();

            int size_needed = WideCharToMultiByte(CP_UTF8, 0, wStem.c_str(), -1, NULL, 0, NULL, NULL);
            std::string utf8Stem(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, wStem.c_str(), -1, &utf8Stem[0], size_needed, NULL, NULL);

            // 문자열 끝의 널(Null) 문자가 포함되어 변환될 수 있으므로 제거해줍니다.
            if (!utf8Stem.empty() && utf8Stem.back() == '\0')
            {
                utf8Stem.pop_back();
            }

            // GWorld의 씬 이름과 ImGui UI의 buffer를 갱신
            strcpy_s(buffer, sizeof(buffer), utf8Stem.c_str());

            AddLog(L"Scene loaded successfully: " + FilePath);

            if (GApplication)
                GApplication->UpdateEditorViewport();
        }
        else
        {
            AddLog(L"Failed to load scene.");
        }
    }
}

void UImGuiManager::SetCameraInfo()
{
    FEditorViewportClient* ViewportClient = GEditor->GetEditorViewportClient();
    FViewportCameraTransform Camera = ViewportClient->GetCameraTransform();

    ImGui::Checkbox("Orthographic", &UImGuiManager::Get().bIsOrthogonal);

    float fovDeg = Camera.GetFOV() * 180.0f / 3.14159265f;

    ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.5f);
    if (ImGui::DragFloat("FOV", &fovDeg, 0.1f, 60.0f, 120.0f))
    {
        Camera.SetFOV(fovDeg * 3.14159265f / 180.0f);
    }

    ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.5f);
    ImGui::DragFloat3("Camera Location", &Camera.GetLocation().X, 0.01f);
    ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.5f);
    ImGui::DragFloat3("Camera Rotation", &Camera.GetRotation().X, 0.1f);

    if (ViewportClient != nullptr)
    {
        ImGui::Separator();
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.5f);
        ImGui::SliderFloat("Move Sensitivity", ViewportClient->GetMoveSpeedPtr(), 0.1f, 100.0f, "%.2f",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.5f);
        ImGui::SliderFloat("Rotation Sensitivity", ViewportClient->GetRotSpeedPtr(), 0.01f, 0.5f, "%.2f",
                           ImGuiSliderFlags_Logarithmic);

        // TODO: Grid Step은 CameraInfo가 아니므로, 함수이름을 바꾸거나 CameraInfo에서 분리하는 것을 고려
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() * 0.5f);
        float GridStep = ViewportClient->GetGridStep();
        if (ImGui::SliderFloat("Grid Step", &GridStep, 0.1f, 10.0f, "%.2f"))
        {
            ViewportClient->SetGridStep(GridStep);
        }
    }
}

void UImGuiManager::TransformInspector()
{
    UObject* SelectedObject = nullptr;
    if (GEditor && GEditor->GetSelection())
    {
        SelectedObject = GEditor->GetSelection()->GetSelectedObject(0);
    }

    // SelectedObject가 Actor인지 Component인지 안전하게 구별하여 Owner를 찾음
    AActor* OwnerActor = Cast<AActor>(SelectedObject);

    // 만약 액터 자체가 아니라 컴포넌트가 선택된 것이라면, 컴포넌트의 Owner를 가져옵니다.
    if (OwnerActor == nullptr)
    {
        if (USceneComponent* SelectedComp = Cast<USceneComponent>(SelectedObject))
        {
            OwnerActor = SelectedComp->GetOwner();
        }
    }

    // 그래도 찾지 못했다면 렌더링 취소
    if (OwnerActor == nullptr || OwnerActor->GetRootComponent() == nullptr)
        return;

    FTransform t = OwnerActor->GetTransform();

    ImGui::DragFloat3("Translation", &t.Location.X, 0.01f);
    ImGui::DragFloat3("Rotation", &t.Rotation.X, 0.01f);
    ImGui::DragFloat3("Scale", &t.Scale.X, 0.01f, 0.f, FLT_MAX);

    if (ImGui::Button("Change Mode"))
        bToggleGizmoMode = true;

    static HIMC s_hPrevImc = NULL;

    // if (UTextComponent* text = Cast<UTextComponent>(SelectedObject))
    //{
    //     if (text->IsExactly(UTextComponent::StaticClass()))
    //     {
    //         strncpy_s(TextBuffer, text->GetText().c_str(), IM_ARRAYSIZE(TextBuffer));
    //         // TextBuffer를 버퍼로 사용
    //         if (ImGui::InputText("text name", TextBuffer, IM_ARRAYSIZE(TextBuffer)))
    //         {
    //             text->SetText(TextBuffer);
    //         }

    //        if (ImGui::IsItemDeactivated())
    //        {
    //            HWND hwnd = ::GetFocus();
    //            if (!hwnd) hwnd = ::GetActiveWindow();
    //            HIMC hImc = ImmGetContext(hwnd);
    //            if (hImc)
    //            {
    //                ImmNotifyIME(hImc, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
    //                ImmReleaseContext(hwnd, hImc);
    //            }
    //        }
    //    }
    //}

    OwnerActor->SetTransform(t);
}

std::wstring UImGuiManager::SaveFileDialog()
{
    OPENFILENAMEW ofn;
    WCHAR szFile[260] = {0};

    ZeroMemory(&ofn, sizeof(OPENFILENAMEW));
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);

    ofn.lpstrFilter = L"Scene Files (*.Scene)\0*.Scene\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;

    // OFN_OVERWRITEPROMPT: 이미 존재하는 파일 선택 시 덮어쓸지 묻는 경고창을
    // 띄웁니다.
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    // 사용자가 확장자를 입력하지 않아도 자동으로 .Scene을 붙여줍니다.
    ofn.lpstrDefExt = L"Scene";

    // 다이얼로그 호출 (Save)
    if (GetSaveFileNameW(&ofn) == TRUE)
    {
        return std::wstring(ofn.lpstrFile);
    }

    return std::wstring(L"");
}

std::wstring UImGuiManager::OpenFileDialog()
{
    OPENFILENAMEW ofn;
    WCHAR szFile[260] = {0};

    ZeroMemory(&ofn, sizeof(OPENFILENAMEW));
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = NULL;

    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);

    // 파일 필터 설정 (.Scene 파일 전용)
    ofn.lpstrFilter = L"Scene Files (*.Scene)\0*.Scene\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;

    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    std::wstring DirectoryPath = L"Data\\Scene";
    if (!std::filesystem::exists(DirectoryPath))
    {
        std::filesystem::create_directories(DirectoryPath);
    }
    std::wstring InitialDir = std::filesystem::absolute(DirectoryPath).wstring();
    ofn.lpstrInitialDir = InitialDir.c_str();

    // 대화상자 호출 (불러오기)
    if (GetOpenFileNameW(&ofn) == TRUE)
    {
        return std::wstring(ofn.lpstrFile);
    }

    return std::wstring(L"");
}
