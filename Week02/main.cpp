#include "CoreTypes.h"
#include "World.h"

#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Editor/Public/Application.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
UWorld         *GWorld = nullptr;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	GWorld = new UWorld();

	UApplication* main_app = new UApplication();

	main_app->Initialize(hInstance);
	main_app->Run();
	main_app->Finish();

	delete main_app;
	delete GWorld;

	return 0;
}