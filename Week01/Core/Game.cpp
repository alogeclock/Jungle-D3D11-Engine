#include "Game.h"

void UGame::Init(URenderer* InRenderer, HWND InHWnd)
{
	Renderer = InRenderer;
	hWnd = InHWnd;
}

void UGame::Update(float deltaTime)
{
	if (Phase == EGamePhase::Title)
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
	}
}

void UGame::Render()
{
	Renderer->Prepare();
	Renderer->PrepareShader();

	// TODO: 게임 오브젝트 렌더링

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	Renderer->SwapBuffer();
}

void UGame::Release()
{
}

void UGame::StartGame()
{
}

void UGame::ShowCredit()
{
}

void UGame::ShowScore()
{
}
