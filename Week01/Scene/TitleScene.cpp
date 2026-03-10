#include "TitleScene.h"
#include "Core/GameContext.h"
#include "Gameplay/CreditPopup.h"
#include "Gameplay/ScoreboardPopup.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"

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

	ImGui::End();

	UIManager.Render(Context);
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
