#include "pch.h"
#include <memory>
#include <string>
#include "Core/GameContext.h"
#include "Render/Renderer.h"
#include "Render/TextureManager.h"
#include "Scene/SceneCommand.h"
#include "Scene/SceneType.h"
#include "TitleScene.h"
#include "UI/popup/CreditPopup.h"
#include "UI/popup/PopupManager.h"
#include "UI/popup/ScoreboardPopup.h"


void FTitleScene::Update(FGameContext &Context)
{
    UIManager.Update(Context);
    UIManager.GetPopupManager().RemoveClosedPopup();
}

void FTitleScene::Render(FGameContext &Context)
{
    RenderBackground(Context);
    RenderTitleMenu(Context);
    UIManager.Render(Context);

#ifdef DEBUG
    ImGui::Begin("Debug Shader");

    ImGui::Text("Shader: %s", Context.Renderer.GetCurrentShaderName().c_str());

    const auto Shaders = Context.Renderer.GetAvailableShaders();
    for (const auto &Name : Shaders)
    {
        const bool bIsCurrent = (Name == Context.Renderer.GetCurrentShaderName());

        if (bIsCurrent)
        {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "> %s", Name.c_str());
        }
        else if (ImGui::Button(Name.c_str()))
        {
            std::wstring Path =
                L"Resources/Shaders/" + std::wstring(Name.begin(), Name.end()) + L".hlsl";
            Context.Renderer.LoadShaderFromFile(Path);
        }
    }

    const std::string &Error = Context.Renderer.GetShaderError();
    if (!Error.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s", Error.c_str());
    }

    ImGui::End();
#endif
}

// 임시
#define WIN_WIDTH 1024
#define WIN_HEIGHT 1024

void FTitleScene::RenderBackground(FGameContext &Context)
{
    Context.Renderer.DrawTexture(Context.Textures.Get("title_background"), 0.0f, 0.0f, WIN_WIDTH, WIN_HEIGHT);
}

void FTitleScene::RenderTitleMenu(FGameContext &Context)
{
    ImGuiIO &Io = ImGui::GetIO();

    const float ScreenWidth = Io.DisplaySize.x;
    const float ScreenHeight = Io.DisplaySize.y;

    // 전체 화면 덮는 투명 윈도우
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(ScreenWidth, ScreenHeight));

    ImGuiWindowFlags WindowFlags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("TitleMenuOverlay", nullptr, WindowFlags);

    HandleMenuCommand(Context);

    ImGui::End();
    ImGui::PopStyleVar();
}

void FTitleScene::HandleMenuCommand(FGameContext &Context)
{
    FPopupManager &PopupManager = UIManager.GetPopupManager();
    const bool     bHasOpenPopup = PopupManager.HasOpenPopup();

    ImGuiIO    &Io = ImGui::GetIO();
    const float ScreenWidth = Io.DisplaySize.x;
    const float ScreenHeight = Io.DisplaySize.y;

    // 버튼 크기와 간격
    const ImVec2 ButtonSize(320.0f, 80.0f);
    const float  ButtonSpacing = 20.0f;
    const float  TotalHeight = ButtonSize.y * 3.0f + ButtonSpacing * 2.0f;

    const float StartX = (ScreenWidth - ButtonSize.x) * 0.5f;
    const float StartY = (ScreenHeight - TotalHeight) * 0.5f;

    // 타이틀 텍스트
    const char *TitleText = "MY GAME";
    ImGui::SetCursorPos(ImVec2((ScreenWidth - 300.0f) * 0.5f, StartY - 120.0f));
    ImGui::PushFont(nullptr); // 큰 폰트 있으면 그걸 push
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s", TitleText);
    ImGui::PopFont();

    // 버튼 스타일
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.10f, 0.10f, 0.75f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.20f, 0.20f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.30f, 0.30f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));

    if (bHasOpenPopup)
    {
        ImGui::BeginDisabled();
    }

    ImGui::SetCursorPos(ImVec2(StartX, StartY));
    if (ImGui::Button("START", ButtonSize))
    {
        FSceneCommand Command;
        Command.Type = ESceneCommandType::ChangeScene;
        Command.NextScene = ESceneType::Play;
        Command.NextStageIndex = 0;
        SetSceneCommand(Command);
    }

    ImGui::SetCursorPos(ImVec2(StartX, StartY + ButtonSize.y + ButtonSpacing));
    if (ImGui::Button("CREDIT", ButtonSize))
    {
        OpenCreditPopup();
    }

    ImGui::SetCursorPos(ImVec2(StartX, StartY + (ButtonSize.y + ButtonSpacing) * 2.0f));
    if (ImGui::Button("SCORE", ButtonSize))
    {
        OpenScoreboardPopup();
    }

    if (bHasOpenPopup)
    {
        ImGui::EndDisabled();
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}

void FTitleScene::OpenCreditPopup()
{
    UIManager.GetPopupManager().Open(std::make_unique<FCreditPopup>());
}

void FTitleScene::OpenScoreboardPopup()
{
    UIManager.GetPopupManager().Open(std::make_unique<FScoreboardPopup>());
}
