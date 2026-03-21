#include "Source/Core/Public/Memory.h"
#include "Source/Editor/Public/Application.h"
#include "World.h"

#include <Windows.h>

UWorld *GWorld = nullptr;
UApplication *GApplication = nullptr;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // ImGui에 입력
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
    {
        return true;
    }

    UApplication *App = reinterpret_cast<UApplication *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    FViewport    *Viewport = App ? App->GetViewport() : nullptr;

    switch (message)
    {
    case WM_DESTROY:
        PostQuitMessage(0); // 프로그램 종료 메시지를 메시지 큐에 넣는다.
        break;
    case WM_SIZE:
    {
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

    WCHAR     WindowClass[] = L"JungleWindowClass";
    WCHAR     Title[] = L"Game Tech Lab";
    WNDCLASSW wndclass = {0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass};

    RegisterClassW(&wndclass);

    hWnd = CreateWindowExW(0, WindowClass, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1400, 800, nullptr, nullptr, hInst,
                           nullptr);

    SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    // Viewport
    if (Viewport != nullptr)
    {
        Viewport->CreateEditorViewportClient();

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

    UTextureManger::Get().Initialize(*Renderer);
    // void* a = UTextureManger::Get().GetTexture("Data/DejaVu Sans Mono.dds");

    UpdateEditorViewport();
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
            if (bResize)
            {
                Renderer->OnResize(Width, Height);
                Viewport->OnResize(Width, Height);
                bResize = false;
            }

            FEditorViewportClient *ViewportClient = Viewport->GetViewportClient();

            FSceneViewOptions ViewOptions;
            ViewOptions.ShowFlags = ViewportClient->GetShowFlags();
            ViewOptions.ViewMode = ViewportClient->GetViewMode();
            ViewOptions.bDrawAABB = ViewportClient->GetDrawAABB();

            Viewport->Tick(UTimeManager::Get().GetDeltaTime());
            Renderer->Prepare(ViewOptions);

            GWorld->Render(*Renderer);
            GWorld->Tick(UTimeManager::Get().GetDeltaTime());

            ViewportClient->Render(*Renderer);

            UImGuiManager::Get().Update();
            UTimeManager::Get().Update();

            Renderer->SwapBuffer();
        }
    }
}

void UApplication::Finish()
{
    UMeshManager::Get().Release(*Renderer);
    UImGuiManager::Get().Release();

    Renderer->ReleaseConstantBuffer();
    Renderer->Release();
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

    // 2. 월드나 현재 레벨이 유효하지 않으면 뷰포트에서 에디터 객체 참조 해제
    if (!GWorld || !GWorld->GetCurrentLevel())
    {
        ViewportClient->SetGrid(nullptr);
        ViewportClient->SetAxis(nullptr);
        ViewportClient->SetGizmo(nullptr);
        return;
    }

    AGrid* FoundGrid = nullptr;
    AAxis* FoundAxis = nullptr;
    APivotTransformGizmo* FoundGizmo = nullptr;

    // 3. 현재 레벨의 Actors 배열을 순회하며 에디터 객체 찾기
    for (AActor* Actor : GWorld->GetCurrentLevel()->GetActors())
    {
        if (!FoundGrid) FoundGrid = dynamic_cast<AGrid*>(Actor);
        if (!FoundAxis) FoundAxis = dynamic_cast<AAxis*>(Actor);
        if (!FoundGizmo) FoundGizmo = dynamic_cast<APivotTransformGizmo*>(Actor);
    }

    // 4. 찾은 객체들을 뷰포트 클라이언트에 맵핑
    ViewportClient->SetGrid(FoundGrid);
    ViewportClient->SetAxis(FoundAxis);
    ViewportClient->SetGizmo(FoundGizmo);
}