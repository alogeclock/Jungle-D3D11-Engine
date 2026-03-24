#include "Source/Engine/Public/GUI/SceneWindow.h"
#include "Source/Editor/Public/Application.h"
#include "Source/Editor/Public/EditorEngine.h"
#include "World.h"

FSceneWindow::FSceneWindow(FString InName) : FGuiWindow(InName)
{
    buffer[0] = '\0';

    if (GWorld && GWorld->GetCurrentLevel())
    {
        snprintf(buffer, sizeof(buffer), "%s", GWorld->GetCurrentLevel()->GetName().ToString().c_str());
    }
}

FGuiMsg FSceneWindow::ShowDetail()
{
    FGuiMsg resultMsg;
    if (buffer[0] == '\0' && GWorld && GWorld->GetCurrentLevel())
    {
        snprintf(buffer, sizeof(buffer), "%s", GWorld->GetCurrentLevel()->GetName().ToString().c_str());
    }

    ImGui::InputText("Scene Name", buffer, IM_ARRAYSIZE(buffer));
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
            }

            if (OldLevel != nullptr)
            {
                GWorld->GetLevels().erase(OldLevel); // UWorld에 GetLevels() 필요
                delete OldLevel;
            }

            if (GApplication)
                GApplication->UpdateEditorViewport();
        }
    }

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
            resultMsg.Msg += "Scene saved successfully: " + WStringToFString(FilePath);
        }
        else
        {
            resultMsg.Msg += "Failed to save scene: " + WStringToFString(FilePath);
            resultMsg.bIsExcetion = true;
        }
    }

    if (ImGui::Button("Load Scene", ImVec2(ImGui::GetContentRegionAvail().x, 0)))
    {
        std::wstring FilePath = OpenFileDialog();

        // 선택된 객체 초기화
        if (GEditor && GEditor->GetSelection())
        {
            GEditor->GetSelection()->Clear();
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

            resultMsg.Msg += "Scene loaded successfully: " + WStringToFString(FilePath);

            if (GApplication)
                GApplication->UpdateEditorViewport();
        }
        else
        {
            // AddLog(L"Failed to load scene.");
            resultMsg.Msg += "Failed to load scene. " + WStringToFString(FilePath);
            resultMsg.bIsExcetion = true;
        }
    }

    return resultMsg;
}

FString FSceneWindow::WStringToFString(const std::wstring& string)
{
    if (string.empty())
        return std::string();

    int size_needed = WideCharToMultiByte(CP_ACP, 0, &string[0], (int)string.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_ACP, 0, &string[0], (int)string.size(), &strTo[0], size_needed, NULL, NULL);

    return strTo;
}

std::wstring FSceneWindow::CharToWString(const char* charStr)
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

std::wstring FSceneWindow::SaveFileDialog()
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

std::wstring FSceneWindow::OpenFileDialog()
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