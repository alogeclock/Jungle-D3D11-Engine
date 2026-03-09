#include "TitleScene.h"
#include "../Core/GameContext.h"
#include "../Gameplay/PopupManager.h"
#include "../Gameplay/CreditPopup.h"
#include "../Gameplay/ScoreboardPopup.h"

#include "../imgui/imgui.h"
#include "../imgui/imgui_internal.h"
#include "../ImGui/imgui_impl_dx11.h"
#include "../imGui/imgui_impl_win32.h"

FTitleScene::~FTitleScene() = default;

void FTitleScene::Enter()
{
	PopupManager = std::make_unique<FPopupManager>();
	PopupManager->AddPopup("Credit", std::make_unique<FCreditPopup>());
	PopupManager->AddPopup("Scoreboard", std::make_unique<FScoreboardPopup>());
}

void FTitleScene::Exit()
{
	PopupManager.reset();
}

void FTitleScene::Update(FGameContext& Context)
{
	if (PopupManager)
	{
		PopupManager->Update(Context);
	}
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

	if (PopupManager)
	{
		PopupManager->Render(Context);
	}
}

void FTitleScene::StartGame()
{
	RequestSceneChange(ESceneType::Play);
}

void FTitleScene::ShowCredit()
{
	if (PopupManager)
	{
		PopupManager->Open("Credit");
	}
}

void FTitleScene::ShowScore()
{
	if (PopupManager)
	{
		PopupManager->Open("Scoreboard");
	}
}
