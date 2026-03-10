#include "pch.h"
#include <windows.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#include "core/Application.h"

// TODO: 맨 처음에 텍스처 한번에 로드
#define WIN_WIDTH 1024
#define WIN_HEIGHT 1024

#ifdef DEBUG
#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#else
#pragma comment(linker, "/subsystem:windows")
#endif

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	FApplication App;

	if (!App.Initialize(hInstance, WIN_WIDTH, WIN_HEIGHT))
		return -1;

	App.Run();
	App.Shutdown();

	return 0;
}
