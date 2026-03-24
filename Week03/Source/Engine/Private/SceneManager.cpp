#include "Source/Engine/Public/SceneManager.h"
#include "Source/Editor/Public/Application.h"
#include "Source/Editor/Public/EditorEngine.h"
#include "World.h"

std::wstring FScenemanager::CharToWString(const char* charStr)
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

FString FScenemanager::WStringToFString(const std::wstring& string)
{
    if (string.empty())
        return std::string();

    int size_needed = WideCharToMultiByte(CP_ACP, 0, &string[0], (int)string.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_ACP, 0, &string[0], (int)string.size(), &strTo[0], size_needed, NULL, NULL);

    return strTo;
}

FScenemanager::FScenemanager()
{
    buffer[0] = '\0';

    if (GWorld && GWorld->GetCurrentLevel())
    {
        snprintf(buffer, sizeof(buffer), "%s", GWorld->GetCurrentLevel()->GetName().ToString().c_str());
    }
}

void FScenemanager::LoadScene(const std::wstring& string)
{
    std::wstring FilePath = string;
    FGuiMsg resultMsg;

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

void FScenemanager::SaveScene(const std::wstring& string)
{
    std::wstring DirectoryPath = string;

    std::wstring SceneName(CharToWString(buffer));
    FGuiMsg resultMsg;


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
