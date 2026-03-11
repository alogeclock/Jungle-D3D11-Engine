#include "pch.h"
#include <memory>
#include <string>
#include "Core/AudioSystem.h"
#include "Core/GameContext.h"
#include "Data/ScoreRepository.h"
#include "Render/Renderer.h"
#include "Render/TextureManager.h"
#include "Scene/SceneType.h"
#include "TitleScene.h"
#include "UI/popup/CreditPopup.h"
#include "UI/popup/PopupManager.h"
#include "UI/popup/ScoreboardPopup.h"
#include "UI/popup/UIPopupAction.h"

#define WIN_WIDTH 1024
#define WIN_HEIGHT 1024

void FTitleScene::Update(FGameContext &Context)
{
    if (!bInitialized)
    {
        FAudioSystem::Get().StopAll();
        FAudioSystem::Get().LoadWav("bgm_title", "Resources/Sounds/title.wav");
        FAudioSystem::Get().Play("bgm_title", true);
        bInitialized = true;
    }

    UIManager.Update(Context);
}

void FTitleScene::Render(FGameContext &Context)
{
    RenderBackground(Context);
    RenderTitleMenu(Context);
    UIManager.Render(Context);

    HandlePopupResult(Context);
    UIManager.GetPopupManager().RemoveClosedPopup();

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

void FTitleScene::RenderBackground(FGameContext &Context)
{
    Context.Renderer.DrawTexture(Context.Textures.Get("title_background"), (WIN_WIDTH / 2.f),
                                 (WIN_HEIGHT / 2.f), WIN_WIDTH, WIN_HEIGHT);
}

void FTitleScene::RenderTitleMenu(FGameContext &Context)
{
    ImGuiIO &Io = ImGui::GetIO();

    const float ScreenWidth = Io.DisplaySize.x;
    const float ScreenHeight = Io.DisplaySize.y;

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

    const ImVec2 ButtonSize(320.0f, 80.0f);
    const float  ButtonSpacing = 20.0f;
    const float  TotalHeight = ButtonSize.y * 3.0f + ButtonSpacing * 2.0f;

    const float StartX = (ScreenWidth - ButtonSize.x) * 0.5f;
    const float StartY = (ScreenHeight - TotalHeight) * 0.5f;

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
        ChangeScene(ESceneType::Play, 0);
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

#ifdef DEBUG
    ImGui::SetCursorPos(ImVec2(StartX, StartY + (ButtonSize.y + ButtonSpacing) * 3.0f));
    if (ImGui::Button("Test Scene", ButtonSize))
    {
        ChangeScene(ESceneType::Test);
    }
#endif

    if (bHasOpenPopup)
    {
        ImGui::EndDisabled();
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}

void FTitleScene::HandlePopupResult(FGameContext &Context)
{
    FPopupManager &PopupManager = UIManager.GetPopupManager();

    if (FCreditPopup *Popup = PopupManager.GetPopup<FCreditPopup>())
    {
        DispatchPopupAction(Context, *Popup, Popup->ConsumeAction());
        return;
    }

    if (FScoreboardPopup *Popup = PopupManager.GetPopup<FScoreboardPopup>())
    {
        DispatchPopupAction(Context, *Popup, Popup->ConsumeAction());
        return;
    }
}

bool FTitleScene::HandleOwnPopupAction(FGameContext &Context, FUIPopupBase &Popup,
                                       EUIPopupAction Action)
{
    return false;
}

void FTitleScene::OpenCreditPopup()
{
    std::unique_ptr<FCreditPopup> Popup = std::make_unique<FCreditPopup>();
    Popup->Open();
    UIManager.GetPopupManager().Open(std::move(Popup));
}

void FTitleScene::OpenScoreboardPopup()
{
    std::unique_ptr<FScoreboardPopup> Popup = std::make_unique<FScoreboardPopup>();
    Popup->SetEntries(ScoreRepository::LoadSorted());
    Popup->ResetPage();
    Popup->Open();

    UIManager.GetPopupManager().Open(std::move(Popup));
}
