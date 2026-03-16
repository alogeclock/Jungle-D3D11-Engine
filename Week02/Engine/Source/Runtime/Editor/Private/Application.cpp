#include "Memory/Memory.h"
#include "Engine/Source/Runtime/Editor/Public/Application.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// WndProc 함수는 각종 메시지를 처리한다.
/* hWnd는 이벤트가 발생한 창의 번호를 의미한다. message는 사건의 종류를 의미한다.
   가령 WM_KEYDOWN은 키 눌림, WM_LBUTTONDOWN은 마우스 클릭,
	 WM_DESTROY는 창 파괴를 의미한다. */
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// ImGui에 입력
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
	{
		return true;
	}

	FViewport* Viewport = reinterpret_cast<FViewport*>(
		GetWindowLongPtr(hWnd, GWLP_USERDATA)
		);

	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0); // 프로그램 종료 메시지를 메시지 큐에 넣는다.
		break;
	case WM_KEYDOWN:
		Viewport->OnKeyDown((uint32_t)wParam);
		break;
	case WM_KEYUP:
		Viewport->OnKeyUp((uint32_t)wParam);
		break;
	case WM_MOUSEMOVE:
		Viewport->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	case WM_LBUTTONDOWN:
		Viewport->OnMouseButtonDown(VK_LBUTTON, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	case WM_RBUTTONDOWN:
		Viewport->OnMouseButtonDown(VK_RBUTTON, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		break;
	case WM_LBUTTONUP:
		Viewport->OnMouseButtonUp(VK_LBUTTON);
		break;
	case WM_RBUTTONUP:
		Viewport->OnMouseButtonUp(VK_RBUTTON);
		break;
	case WM_MOUSEWHEEL:
		Viewport->OnMouseWheel((float)GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}

UApplication::UApplication()
{
	Renderer = new URenderer();
	Viewport = new FViewport();
}

UApplication::~UApplication()
{
	delete Renderer;
	delete Viewport;
}

void UApplication::Initialize(HINSTANCE hInstance)
{
	hInst = hInstance;

	WCHAR WindowClass[] = L"JungleWindowClass";
	WCHAR Title[] = L"Game Tech Lab";
	WNDCLASSW wndclass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };

	RegisterClassW(&wndclass);

	hWnd = CreateWindowExW(0, WindowClass, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1024, 1024,
		nullptr, nullptr, hInst, nullptr);

	// Viewport
	SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(Viewport));

	if(Viewport != nullptr)
		Viewport->CreateEditorViewportClient();

	// Rendering
    Renderer->SetViewport(Viewport);
	Renderer->Create(hWnd);
	
	// Mesh Manager
    UMeshManager::Get().Initialize(*Renderer);

    MainAxis = new UAxis();
	cube = new UCubeComponent();
	ring = new URingComponent();
	sphere = new USphereComponent(0.2f);
	gizmo = new APivotTransformGizmo();

	// ImGui
	UImGuiManager::Get().Create(hWnd, Renderer);
    UImGuiManager::Get().SetSelectedObject(sphere); // 피킹 후 Set으로 나중에 순서 변경

	// Timer
    UTimeManager::Get().Initialize();
}

void UApplication::Run()
{
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
			Viewport->Tick(UTimeManager::Get().GetDeltaTime());
			Renderer->Prepare();

			MainAxis->Render(*Renderer);
			cube->Render(*Renderer);
			ring->Render(*Renderer);
			sphere->Render(*Renderer);
			gizmo->Render(*Renderer);

			UImGuiManager::Get().Update();
            UTimeManager::Get().Update();

			Renderer->SwapBuffer();
		}
	}
}

void UApplication::Finish()
{
	Renderer->ReleaseConstantBuffer();
	Renderer->Release();
}

