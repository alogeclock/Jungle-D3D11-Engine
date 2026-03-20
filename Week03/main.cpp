#include "CoreTypes.h"
#include "World.h"

#include "Source/Core/Public/Memory.h"
#include "Source/Editor/Public/Application.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	UApplication* main_app = new UApplication();

	main_app->Initialize(hInstance);
	main_app->Run();
	main_app->Finish();

	delete main_app;
	delete GWorld;

	return 0;
}