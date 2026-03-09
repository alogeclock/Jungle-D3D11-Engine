#pragma once

#include <windows.h>

#include "../imgui/imgui.h"
#include "../ImGui/imgui_impl_dx11.h"

#include "Renderer.h"

enum class EGamePhase
{
	Title,
	Scene1,
	Scene2,
	Scene3,
	StageClear,
};

class UGame
{
public:
	URenderer* Renderer;
	HWND hWnd;
	EGamePhase Phase;

	UGame() : Renderer(nullptr), hWnd(nullptr), Phase(EGamePhase::Title) {}

	void Init(URenderer* InRenderer, HWND InHWnd);
	void Update(float deltaTime);
	void Render();
	void Release();

	void StartGame();
	void ShowCredit();
	void ShowScore();
};
