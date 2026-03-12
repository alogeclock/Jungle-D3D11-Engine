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
        FAudioSystem::Get().Play("bgm_title", true, EAudioChannel::BGM);
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
}

void FTitleScene::RenderBackground(FGameContext &Context)
{
    Context.Renderer.DrawTexture(Context.Textures.Get("title_background"), (WIN_WIDTH / 2.f),
                                 (WIN_HEIGHT / 2.f), WIN_WIDTH, WIN_HEIGHT);
    //Context.Renderer.DrawTexture(Context.Textures.Get("logo"), (WIN_WIDTH) / 2.f,
    //                             (WIN_HEIGHT) / 5.5f, 550, 250);
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
    const float  ButtonSpacing = 32.0f;
    const float  TotalHeight = ButtonSize.y * 4.0f + ButtonSpacing * 3.0f;

    const float StartX = (ScreenWidth - ButtonSize.x) * 0.5f;
    const float StartY = (ScreenHeight - TotalHeight) * 0.5f + 150.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 8.0f));

    ImGui::SetWindowFontScale(1.35f);

    if (bHasOpenPopup)
    {
        ImGui::BeginDisabled();
    }

    // ---------- START GAME : RED ----------
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.03f, 0.03f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.34f, 0.06f, 0.06f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.45f, 0.10f, 0.10f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.75f, 0.20f, 0.20f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
    ImGui::SetCursorPos(ImVec2(StartX, StartY));
    if (ImGui::Button("S T A R T   G A M E", ButtonSize))
    {
        ChangeScene(ESceneType::Play, 0, 0, "player_otaku");
    }
    ImGui::PopStyleColor(5);

    // ---------- SCORE : GREEN ----------
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.02f, 0.16f, 0.06f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.05f, 0.25f, 0.10f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.08f, 0.34f, 0.14f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.75f, 0.35f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
    ImGui::SetCursorPos(ImVec2(StartX, StartY + ButtonSize.y + ButtonSpacing));
    if (ImGui::Button("S C O R E", ButtonSize))
    {
        OpenScoreboardPopup();
    }
    ImGui::PopStyleColor(5);

    // ---------- CREDIT : GREEN ----------
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.02f, 0.16f, 0.06f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.05f, 0.25f, 0.10f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.08f, 0.34f, 0.14f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.75f, 0.35f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
    ImGui::SetCursorPos(ImVec2(StartX, StartY + (ButtonSize.y + ButtonSpacing) * 2.0f));
    if (ImGui::Button("C R E D I T", ButtonSize))
    {
        OpenCreditPopup();
    }
    ImGui::PopStyleColor(5);

#ifdef _DEBUG
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.02f, 0.16f, 0.06f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.05f, 0.25f, 0.10f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.08f, 0.34f, 0.14f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.75f, 0.35f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
    ImGui::SetCursorPos(ImVec2(StartX, StartY + (ButtonSize.y + ButtonSpacing) * 4.0f));
    if (ImGui::Button("T E S T   S C E N E", ButtonSize))
    {
        ChangeScene(ESceneType::Test);
    }
    ImGui::PopStyleColor(5);
#endif

    if (bHasOpenPopup)
    {
        ImGui::EndDisabled();
    }

    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleVar(3);
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
    std::vector<FCreditEntry>     Credits;
    if (FCreditLoader::Get().LoadCredits(Credits))
        Popup->SetCredits(Credits);
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
