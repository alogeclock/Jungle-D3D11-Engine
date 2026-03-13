#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "imGui/imgui_impl_win32.h"

#include "Engine/Source/Runtime/Engine/Public/Rendering/Renderer.h"
#include "Engine/Source/Runtime/Engine/Public/Classes/Components/AxisComponent.h"
#include "Engine/Source/Runtime/Engine/Public/ImGuiManager.h"
#include "Engine/Source/Runtime/Core/Public/Cube.h"
#include <iostream>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam);

// WndProc 함수는 각종 메시지를 처리한다.
/* hWnd는 이벤트가 발생한 창의 번호를 의미한다. message는 사건의 종류를 의미한다.
   가령 WM_KEYDOWN은 키 눌림, WM_LBUTTONDOWN은 마우스 클릭,
	 WM_DESTROY는 창 파괴를 의미한다. */
LRESULT CALLBACK WndProc(HWND hWnd, uint32 message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
	{
		return true;
	}

	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0); // 프로그램 종료 메시지를 메시지 큐에 넣는다.
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	WCHAR WindowClass[] = L"JungleWindowClass";
	WCHAR Title[] = L"Game Tech Lab";
	// wndProc의 함수 포인터를 WindowClass 구조체 안에 넣는다.
	WNDCLASSW wndclass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };

	RegisterClassW(&wndclass);

	HWND hWnd = CreateWindowExW(0, WindowClass, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1024, 1024,
		nullptr, nullptr, hInstance, nullptr);

	// Rendering
	URenderer renderer;
	renderer.Create(hWnd);
	
	// World Axis
	UAxisComponent* MainAxis = new UAxisComponent();
	ID3D11Buffer* VertexBuffer = renderer.CreateVertexBuffer(MainAxis->GetVertexData(), MainAxis->GetVertexByteWidth());
	MainAxis->SetVertexBuffer(VertexBuffer);

	// Test Object -> 나중에 이동
	UINT numVerticesSphere = sizeof(cube_vertices) / sizeof(FVertexSimple);
	ID3D11Buffer* vertexBufferSphere = renderer.CreateVertexBuffer(cube_vertices, sizeof(cube_vertices));
	USphere* sphere = new USphere();

	// ImGui
	UImGuiManager::Get().Create(hWnd, &renderer);
	UImGuiManager::Get().SetSelectedObject(sphere);

	// Main Loop
	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			renderer.Prepare();
			
			sphere->Render(renderer);
			renderer.RenderPrimitive(vertexBufferSphere, numVerticesSphere);

			FConstants basic;
			basic.worldMatrix = FMatrix<float>::Identity();
			renderer.UpdateConstant(basic);
			MainAxis->RenderPrimitive(renderer);

			UImGuiManager::Get().Update();

			renderer.SwapBuffer();
		}
	}

	delete MainAxis;

	renderer.ReleaseVertexBuffer(VertexBuffer);
	renderer.ReleaseConstantBuffer();
	renderer.Release();

	return 0;
}