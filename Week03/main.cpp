#include "CoreTypes.h"
#include "World.h"

#include "Source/Core/Public/Memory.h"
#include "Source/Editor/Public/Application.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
#ifdef _DEBUG
	// 디버그 모드에서만 콘솔 창을 할당합니다.
	AllocConsole();
	FILE* File = nullptr;
	freopen_s(&File, "CONOUT$", "w", stdout);

	std::cout << "Engine Initialization Started..." << std::endl;
#endif

	UApplication* main_app = new UApplication();

	main_app->Initialize(hInstance);
	main_app->Run();
	main_app->Finish();

	delete main_app;

#ifdef _DEBUG
	// 프로그램 종료 전에 콘솔을 해제합니다.
	FreeConsole();
#endif

	return 0;
}