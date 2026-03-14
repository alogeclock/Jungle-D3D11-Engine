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
		Viewport->OnMouseMove(LOWORD(lParam), HIWORD(lParam));
		break;
	case WM_LBUTTONDOWN:
		Viewport->OnMouseButtonDown(VK_LBUTTON, LOWORD(lParam), HIWORD(lParam));
		break;
	case WM_LBUTTONUP:
		Viewport->OnMouseButtonUp(VK_LBUTTON);
		break;
	case WM_RBUTTONDOWN:
		Viewport->OnMouseButtonDown(VK_RBUTTON, LOWORD(lParam), HIWORD(lParam));
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
	// wndProc의 함수 포인터를 WindowClass 구조체 안에 넣는다.
	WNDCLASSW wndclass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };

	RegisterClassW(&wndclass);

	hWnd = CreateWindowExW(0, WindowClass, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 1024, 1024,
		nullptr, nullptr, hInst, nullptr);

	SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(Viewport));

	if(Viewport != nullptr)
		Viewport->CreateEditorViewportClient();

	// Rendering
	Renderer->Create(hWnd);
	Renderer->SetViewport(Viewport);

	// World Axis
	MainAxis = new UAxisComponent();
	VertexBuffer = Renderer->CreateVertexBuffer(MainAxis->GetVertexData(), MainAxis->GetVertexByteWidth());
	MainAxis->SetVertexBuffer(VertexBuffer);

	UBoxComponent* box = new UBoxComponent();
	ID3D11Buffer* vertexBufferBox = Renderer->CreateVertexBuffer(box->GetVertices(), box->GetVertexByteWidth());
    box->SetVertexBuffer(vertexBufferBox);

	// Test Object -> 나중에 이동
	sphere = new USphereComponent(0.2f);
	vertexBufferSphere = Renderer->CreateVertexBuffer(sphere->GetVertices(), sphere->GetVertexByteWidth());
    sphere->SetVertexBuffer(vertexBufferSphere);

	// ImGui
	UImGuiManager::Get().Create(hWnd, Renderer);
	UImGuiManager::Get().SetSelectedObject(sphere);
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
			float DeltaTime = 1.0f / 60.0f;

			Viewport->Tick(DeltaTime);
			Renderer->Prepare();

			sphere->Render(*Renderer);

			FConstants basic;
			basic.worldMatrix = FMatrix<float>::Identity();
			Renderer->UpdateConstant(basic);
			MainAxis->RenderPrimitive(*Renderer);

			UImGuiManager::Get().Update();

			Renderer->SwapBuffer();
		}
	}
}

void UApplication::Finish()
{
	delete MainAxis;

	Renderer->ReleaseVertexBuffer(VertexBuffer);
	Renderer->ReleaseConstantBuffer();
	Renderer->Release();
}

