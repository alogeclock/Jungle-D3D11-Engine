#include "Source/Editor/Public/Application.h"
#include "Source/Core/Public/Memory.h"
#include "World.h"

#include <Windows.h>

UApplication* GApplication = nullptr;
UEditorEngine* GEditor = nullptr;
FScene* GMainScene = nullptr;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // ImGui에 입력
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
    {
        return true;
    }

    UApplication* App = reinterpret_cast<UApplication*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    FViewport* Viewport = App ? App->GetViewport() : nullptr;

    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0); // 프로그램 종료 메시지를 메시지 큐에 넣는다.
        break;
    case WM_SIZE: {
        if (wParam == SIZE_MINIMIZED)
            break;
        if (!App)
            break;

        uint32 NewWidth = LOWORD(lParam);
        uint32 NewHeight = HIWORD(lParam);

        if (App && NewWidth > 0 && NewHeight > 0)
            App->OnResize(NewWidth, NewHeight);

        break;
    }
    case WM_KEYDOWN:
        if (Viewport)
            Viewport->OnKeyDown((uint32_t)wParam);
        break;
    case WM_KEYUP:
        if (Viewport)
            Viewport->OnKeyUp((uint32_t)wParam);
        break;
    case WM_MOUSEMOVE:
        if (Viewport)
            Viewport->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        break;
    case WM_LBUTTONDOWN:
        if (Viewport)
            Viewport->OnMouseButtonDown(VK_LBUTTON, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        break;
    case WM_RBUTTONDOWN:
        if (Viewport)
            Viewport->OnMouseButtonDown(VK_RBUTTON, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        break;
    case WM_LBUTTONUP:
        if (Viewport)
            Viewport->OnMouseButtonUp(VK_LBUTTON);
        break;
    case WM_RBUTTONUP:
        if (Viewport)
            Viewport->OnMouseButtonUp(VK_RBUTTON);
        break;
    case WM_MOUSEWHEEL:
        if (Viewport)
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
    GMainScene = new FScene();
    GEditor = new UEditorEngine("Editor");
    GWorld = new UWorld("World");
    GApplication = this;
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
    WNDCLASSW wndclass = {0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass};

    RegisterClassW(&wndclass);

    hWnd = CreateWindowExW(0, WindowClass, Title, WS_MAXIMIZE | WS_VISIBLE | WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                           CW_USEDEFAULT, 1400, 800, nullptr, nullptr, hInst, nullptr);

    SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    // Viewport
    if (Viewport != nullptr)
    {
        FEditorViewportClient* ViewportClient = new FEditorViewportClient(Viewport);
        Viewport->SetEditorViewportClient(ViewportClient);

        RECT rect;
        GetClientRect(hWnd, &rect);

        uint32 Width = rect.right - rect.left;
        uint32 Height = rect.bottom - rect.top;

        Viewport->OnResize(Width, Height);
    }

    // Rendering
    Renderer->SetViewport(Viewport);
    Renderer->Create(hWnd);

    // Mesh Manager
    UMeshManager::Get().Initialize(*Renderer);

    // ImGui
    UImGuiManager::Get().Create(hWnd, Renderer);

    // Timer
    UTimeManager::Get().Initialize();

    UTextureManager::Get().Initialize(*Renderer);
    // void* a = UTextureManager::Get().GetTexture("Data/DejaVu Sans Mono.dds");

    UpdateEditorViewport();
}

void UApplication::Run()
{
    // Main Loop
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        // 1. 입력 및 운영체제 메시지 처리
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        // 2. 시간 갱신 (프레임 시작)
        else
        {
            UTimeManager::Get().Update();
            float DeltaTime = UTimeManager::Get().GetDeltaTime();

            Tick(DeltaTime);

            Render();
        }
    }
}

void UApplication::Tick(float DeltaTime)
{
    // 1. 창 리사이즈
    if (bResize)
    {
        Renderer->OnResize(Width, Height);
        Viewport->OnResize(Width, Height);
        bResize = false;
    }

    FEditorViewportClient* ViewportClient = Viewport->GetViewportClient();
    FSceneViewOptions ViewOptions = ViewportClient->GetViewOptions();

    // 2. 객체 갱신
    GEditor->Tick(UTimeManager::Get().GetDeltaTime(), ViewOptions);
    GWorld->Tick(UTimeManager::Get().GetDeltaTime());
}

void UApplication::Render()
{
    FEditorViewportClient* ViewportClient = Viewport->GetViewportClient();
    FSceneViewOptions ViewOptions = ViewportClient->GetViewOptions();

    GWorld->Submit(ViewOptions);

    Renderer->Prepare(ViewOptions);
    
    GWorld->GetLineBatcherComponent()->Render(*Renderer);
    GWorld->GetLineBatcherComponent()->Flush();

    Renderer->RenderScene(GMainScene);
    
    GEditor->GetGrid()->GetGridComponent()->Render(*Renderer);
    
    GEditor->Render(*Renderer);
    

    GWorld->GetTextBatcherComponent()->SetViewOption(ViewOptions);
    GWorld->GetTextBatcherComponent()->Render(*Renderer);
    GWorld->GetTextBatcherComponent()->Flush(*Renderer);

    // ViewportClient->Render(*Renderer);
    UImGuiManager::Get().Update();

    Renderer->SwapBuffer();
}

void UApplication::Finish()
{
    UMeshManager::Get().Release(*Renderer);
    UImGuiManager::Get().Release();

    Renderer->ReleaseConstantBuffer();
    Renderer->Release();

	delete GEditor;
    GEditor = nullptr;

    delete GWorld;
    GWorld = nullptr;

    delete GMainScene;
    GMainScene = nullptr;
}

void UApplication::OnResize(uint32 NewWidth, uint32 NewHeight)
{
    bResize = true;
    Width = NewWidth;
    Height = NewHeight;
}

void UApplication::UpdateEditorViewport()
{
    // 1. 뷰포트 및 뷰포트 클라이언트 유효성 검사
    if (!Viewport || !Viewport->GetViewportClient())
        return;

    FEditorViewportClient* ViewportClient = Viewport->GetViewportClient();
}
