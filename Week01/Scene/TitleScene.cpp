#include "pch.h"
#include <memory>
#include <string>
#include "Core/GameContext.h"
#include "Render/Renderer.h"
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
    HandleMenuCommand();
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

void FTitleScene::HandleMenuCommand()
{
    ImGui::Begin("Main Menu");

    FPopupManager &PopupManager = UIManager.GetPopupManager();
    const bool     bHasOpenPopup = PopupManager.HasOpenPopup();

    if (bHasOpenPopup)
    {
        ImGui::BeginDisabled();
    }

    if (ImGui::Button("Start"))
    {
        FSceneCommand Command;
        Command.Type = ESceneCommandType::ChangeScene;
        Command.NextScene = ESceneType::Play;
        Command.NextStageIndex = 0;
        SetSceneCommand(Command);
    }

    if (ImGui::Button("Credit"))
    {
        OpenCreditPopup();
    }

    if (ImGui::Button("Score"))
    {
        OpenScoreboardPopup();
    }

    if (bHasOpenPopup)
    {
        ImGui::EndDisabled();
    }

    ImGui::End();
}

void FTitleScene::OpenCreditPopup()
{
    UIManager.GetPopupManager().Open(std::make_unique<FCreditPopup>());
}

void FTitleScene::OpenScoreboardPopup()
{
    UIManager.GetPopupManager().Open(std::make_unique<FScoreboardPopup>());
}
