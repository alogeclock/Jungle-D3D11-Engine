#include "pch.h"
#include "TitleScene.h"
#include "Core/GameContext.h"
#include "Render/Renderer.h"
#include "Gameplay/CreditPopup.h"
#include "Gameplay/ScoreboardPopup.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#include "Render/Renderer.h"
#include "Render/TextureManager.h"
FTitleScene::~FTitleScene() = default;

void FTitleScene::Enter()
{
	UIManager.GetPopupManager().AddPopup("Credit", std::make_unique<FCreditPopup>());
	UIManager.GetPopupManager().AddPopup("Scoreboard", std::make_unique<FScoreboardPopup>());
}

void FTitleScene::Exit()
{
	UIManager.ClearAll();
}

void FTitleScene::Update(FGameContext& Context)
{
	UIManager.Update(Context);
}

void FTitleScene::Render(FGameContext& Context)
{
	ImGui::Begin("Main Menu");

	if (ImGui::Button("Start"))
	{
		StartGame();
	}

	if (ImGui::Button("Credit"))
	{
		ShowCredit();
	}

	if (ImGui::Button("Score"))
	{
		ShowScore();
	}

#ifdef DEBUG
	ImGui::Separator();
	ImGui::Text("Shader: %s", Context.Renderer.GetCurrentShaderName().c_str());

	auto Shaders = Context.Renderer.GetAvailableShaders();
	for (const auto& Name : Shaders)
	{
		bool bIsCurrent = (Name == Context.Renderer.GetCurrentShaderName());
		if (bIsCurrent)
		{
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "> %s", Name.c_str());
		}
		else if (ImGui::Button(Name.c_str()))
		{
			std::wstring Path = L"Resources/Shaders/" + std::wstring(Name.begin(), Name.end()) + L".hlsl";
			Context.Renderer.LoadShaderFromFile(Path);
		}
	}

	const auto& Error = Context.Renderer.GetShaderError();
	if (!Error.empty())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Error: %s", Error.c_str());
	}
#endif

	ImGui::End();

	UIManager.Render(Context);

	Context.Renderer.DrawTexture(Context.Textures.Get("Floor"), 100, 100, 100, 100);
}

void FTitleScene::StartGame()
{
	RequestSceneChange(ESceneType::Play);
}

void FTitleScene::ShowCredit()
{
	UIManager.GetPopupManager().Open("Credit");
}

void FTitleScene::ShowScore()
{
	UIManager.GetPopupManager().Open("Scoreboard");
}
