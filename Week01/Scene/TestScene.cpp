#include "pch.h"
#include <memory>
#include <string>
#include "Core/GameContext.h"
#include "Data/ScoreRepository.h"
#include "Scene/SceneCommand.h"
#include "Scene/SceneType.h"
#include "TestScene.h"
#include "UI/popup/CreditPopup.h"
#include "UI/popup/GameOverPopup.h"
#include "UI/popup/GoToTitlePopup.h"
#include "UI/popup/PopupManager.h"
#include "UI/popup/SaveScorePopup.h"
#include "UI/popup/ScoreboardPopup.h"
#include "UI/popup/StageClearPopup.h"
#include "UI/popup/UIPopupAction.h"

#define WIN_WIDTH 1024
#define WIN_HEIGHT 1024

void FTestScene::Update(FGameContext &Context) { UIManager.Update(Context); }

void FTestScene::Render(FGameContext &Context)
{
    RenderBackground(Context);
    RenderMenu(Context);
    UIManager.Render(Context);

    HandlePopupResult(Context);
    UIManager.GetPopupManager().RemoveClosedPopup();
}

void FTestScene::RenderBackground(FGameContext &Context)
{
    Context.Renderer.DrawTexture(Context.Textures.Get("test"), 0.0f, 0.0f, 320, 320);
}

void FTestScene::RenderMenu(FGameContext &Context)
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
    ImGui::Begin("TestOverlay", nullptr, WindowFlags);

    HandleMenuCommand(Context);

    ImGui::End();
    ImGui::PopStyleVar();
}

void FTestScene::HandleMenuCommand(FGameContext &Context)
{
    FPopupManager &PopupManager = UIManager.GetPopupManager();
    const bool     bHasOpenPopup = PopupManager.HasOpenPopup();

    ImGuiIO    &Io = ImGui::GetIO();
    const float ScreenWidth = Io.DisplaySize.x;
    const float ScreenHeight = Io.DisplaySize.y;

    const ImVec2 ButtonSize(280.0f, 56.0f);
    const float  ButtonSpacing = 16.0f;
    const float  ColumnSpacing = 24.0f;

    const float TotalWidth = ButtonSize.x * 2.0f + ColumnSpacing;
    const float TotalHeight = ButtonSize.y * 4.0f + ButtonSpacing * 3.0f;

    const float StartX = (ScreenWidth - TotalWidth) * 0.5f;
    const float StartY = (ScreenHeight - TotalHeight) * 0.5f;

    ImGui::SetCursorPos(ImVec2((ScreenWidth - 360.0f) * 0.5f, StartY - 120.0f));
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "POPUP ACTION TEST");

    ImGui::SetCursorPos(ImVec2((ScreenWidth - 420.0f) * 0.5f, StartY - 70.0f));
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.4f, 1.0f), "Last Action: %s", LastActionText.c_str());

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

    const float RightColumnX = StartX + ButtonSize.x + ColumnSpacing;

    ImGui::SetCursorPos(ImVec2(StartX, StartY));
    if (ImGui::Button("CREDIT POPUP", ButtonSize))
    {
        OpenCreditPopup();
    }

    ImGui::SetCursorPos(ImVec2(StartX, StartY + (ButtonSize.y + ButtonSpacing) * 1.0f));
    if (ImGui::Button("SCOREBOARD POPUP", ButtonSize))
    {
        OpenScoreboardPopup();
    }

    ImGui::SetCursorPos(ImVec2(StartX, StartY + (ButtonSize.y + ButtonSpacing) * 2.0f));
    if (ImGui::Button("GAME OVER POPUP", ButtonSize))
    {
        OpenGameOverPopup();
    }

    ImGui::SetCursorPos(ImVec2(StartX, StartY + (ButtonSize.y + ButtonSpacing) * 3.0f));
    if (ImGui::Button("SAVE SCORE POPUP", ButtonSize))
    {
        OpenSaveScorePopup();
    }

    ImGui::SetCursorPos(ImVec2(RightColumnX, StartY));
    if (ImGui::Button("STAGE CLEAR POPUP", ButtonSize))
    {
        OpenStageClearPopup(false);
    }

    ImGui::SetCursorPos(ImVec2(RightColumnX, StartY + (ButtonSize.y + ButtonSpacing) * 1.0f));
    if (ImGui::Button("ALL CLEAR POPUP", ButtonSize))
    {
        OpenStageClearPopup(true);
    }

    ImGui::SetCursorPos(ImVec2(RightColumnX, StartY + (ButtonSize.y + ButtonSpacing) * 2.0f));
    if (ImGui::Button("GO TO TITLE POPUP", ButtonSize))
    {
        OpenGoToTitlePopup();
    }

    ImGui::SetCursorPos(ImVec2(RightColumnX, StartY + (ButtonSize.y + ButtonSpacing) * 3.0f));
    if (ImGui::Button("BACK TO TITLE SCENE", ButtonSize))
    {
        FSceneCommand Command;
        Command.Type = ESceneCommandType::ChangeScene;
        Command.NextScene = ESceneType::Title;
        SetSceneCommand(Command);
    }

    if (bHasOpenPopup)
    {
        ImGui::EndDisabled();
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
}

void FTestScene::HandlePopupResult(FGameContext &Context)
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

    if (FGameOverPopup *Popup = PopupManager.GetPopup<FGameOverPopup>())
    {
        DispatchPopupAction(Context, *Popup, Popup->ConsumeAction());
        return;
    }

    if (FStageClearPopup *Popup = PopupManager.GetPopup<FStageClearPopup>())
    {
        DispatchPopupAction(Context, *Popup, Popup->ConsumeAction());
        return;
    }

    if (FSaveScorePopup *Popup = PopupManager.GetPopup<FSaveScorePopup>())
    {
        DispatchPopupAction(Context, *Popup, Popup->ConsumeAction());
        return;
    }

    if (FGoToTitlePopup *Popup = PopupManager.GetPopup<FGoToTitlePopup>())
    {
        DispatchPopupAction(Context, *Popup, Popup->ConsumeAction());
        return;
    }
}

void FTestScene::OpenCreditPopup()
{
    std::unique_ptr<FCreditPopup> Popup = std::make_unique<FCreditPopup>();
    Popup->Open();
    UIManager.GetPopupManager().Open(std::move(Popup));
}

void FTestScene::OpenScoreboardPopup()
{
    std::unique_ptr<FScoreboardPopup> Popup = std::make_unique<FScoreboardPopup>();
    Popup->SetEntries(ScoreRepository::LoadSorted());
    Popup->ResetPage();
    Popup->Open();

    UIManager.GetPopupManager().Open(std::move(Popup));
}

void FTestScene::OpenGameOverPopup()
{
    std::unique_ptr<FGameOverPopup> Popup = std::make_unique<FGameOverPopup>();
    Popup->Open();
    UIManager.GetPopupManager().Open(std::move(Popup));
}

void FTestScene::OpenStageClearPopup(bool bAllCleared)
{
    std::unique_ptr<FStageClearPopup> Popup = std::make_unique<FStageClearPopup>();
    Popup->SetData(bAllCleared, 3);
    Popup->Open();
    UIManager.GetPopupManager().Open(std::move(Popup));
}

// DEBUG
void FTestScene::OpenSaveScorePopup()
{
    std::unique_ptr<FSaveScorePopup> Popup = std::make_unique<FSaveScorePopup>();
    Popup->SetScore(99999);
    Popup->SetStage(42);
    Popup->Open();
    UIManager.GetPopupManager().Open(std::move(Popup));
}

void FTestScene::OpenGoToTitlePopup()
{
    std::unique_ptr<FGoToTitlePopup> Popup = std::make_unique<FGoToTitlePopup>();
    Popup->Open();
    UIManager.GetPopupManager().Open(std::move(Popup));
}

void FTestScene::SetLastActionText(const char *Text) { LastActionText = Text; }

bool FTestScene::HandleOwnPopupAction(FGameContext &Context, FUIPopupBase &Popup,
                                      EUIPopupAction Action)
{
    switch (Action)
    {
    case EUIPopupAction::OpenSaveScorePopup:
        Popup.Close();
        OpenSaveScorePopup();
        return true;

    case EUIPopupAction::GoToNextStage:
        Popup.Close();
        ChangeScene(ESceneType::Play, 1);
        return true;


    case EUIPopupAction::ConfirmSaveScore:
    {
        Popup.Close();

        FSaveScorePopup *SavePopup = dynamic_cast<FSaveScorePopup *>(&Popup);
        if (!SavePopup)
            return true;

        const std::string Nickname = SavePopup->GetNickname();
        const int         ClearedStage = 42;
        const int         Score = 99999;

        ScoreRepository::AppendRecord({Nickname, ClearedStage, Score});
        OpenGoToTitlePopup();
        return true;
    }

    default:
        return false;
    }
}

void FTestScene::OnPopupActionDispatched(EUIPopupAction Action)
{
    SetLastActionText(ToActionText(Action));
}

const char *FTestScene::ToActionText(EUIPopupAction Action)
{
    switch (Action)
    {
    case EUIPopupAction::None:
        return "None";
    case EUIPopupAction::ClosePopup:
        return "ClosePopup";
    case EUIPopupAction::OpenSaveScorePopup:
        return "OpenSaveScorePopup";
    case EUIPopupAction::ConfirmSaveScore:
        return "ConfirmSaveScore";
    case EUIPopupAction::OpenGoToTitlePopup:
        return "OpenGoToTitlePopup";
    case EUIPopupAction::GoToNextStage:
        return "GoToNextStage";
    case EUIPopupAction::GoToTitleScene:
        return "GoToTitleScene";
    default:
        return "Unknown";
    }
}