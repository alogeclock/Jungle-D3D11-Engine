#pragma once

#include "Engine/Source/Runtime/Editor/Public/Application.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{

	UApplication* main_app = new UApplication();

	main_app->Initialize(hInstance);
	main_app->Run();
	main_app->Finish();

	return 0;
}