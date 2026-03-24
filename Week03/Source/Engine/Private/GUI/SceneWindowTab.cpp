#include "Source/Engine/Public/GUI/SceneWindowTab.h"
#include "Source/Engine/Public/SceneManager.h"
#include <filesystem>

FSceneWindowTab::FSceneWindowTab()
{
    CurrentPath = L"Data/Scene";
}

void FSceneWindowTab::LoadFilePath()
{
    NameToPath.clear();

    std::filesystem::path DirPath = CurrentPath;

    // 폴더 없으면 생성
    if (!std::filesystem::exists(DirPath))
    {
        std::filesystem::create_directories(DirPath);
        return;
    }

    // 재귀 탐색
    for (const auto& Entry : std::filesystem::recursive_directory_iterator(
             DirPath, std::filesystem::directory_options::skip_permission_denied))
    {
        if (!Entry.is_regular_file())
            continue;

        std::filesystem::path FilePath = Entry.path();

        // 확장자 체크 (대소문자 대응)
        std::wstring ext = FilePath.extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

        if (ext != L".scene")
            continue;

        // 파일 이름 (확장자 제거)
        std::wstring FileNameW = FilePath.stem().wstring();

        // wstring → FString
        FString FileName(FileNameW.begin(), FileNameW.end());

        // 전체 경로 (절대경로 추천)
        std::wstring FullPath = std::filesystem::absolute(FilePath).wstring();

        // Map에 저장
        NameToPath[FileName] = FullPath;
    }
}

void FSceneWindowTab::ShowWindow()
{
    if (ImGui::Button("Reload"))
        LoadFilePath();

    FString SceneName;
    for (const auto& pair : NameToPath)
    {
        SceneName = pair.first;
        if (ImGui::Button(SceneName.c_str()))
        {
            GSceneManager.LoadScene(NameToPath[SceneName]);
        }
    }
}
