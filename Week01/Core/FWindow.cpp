#include "FWindow.h"
#include "imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// TODO: 클래스 네임, 창 제목 변경

bool FWindow::Initialize(HINSTANCE InInstance, int InWidth, int InHeight, const wchar_t *InTitle)
{
    InstanceHandle = InInstance;
    Width = InWidth;
    Height = InHeight;

    const wchar_t *ClassName = L"MyWindowClass";

    WNDCLASSEX Wc{};
    Wc.cbSize = sizeof(WNDCLASSEX);
    Wc.style = CS_HREDRAW | CS_VREDRAW;
    Wc.lpfnWndProc = FWindow::WindowProc;
    Wc.cbClsExtra = 0;
    Wc.cbWndExtra = 0;
    Wc.hInstance = InstanceHandle;
    Wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    Wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    Wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    Wc.lpszMenuName = nullptr;
    Wc.lpszClassName = ClassName;
    Wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    if (!RegisterClassEx(&Wc))
        return false;

    RECT Rect{0, 0, Width, Height};
    AdjustWindowRect(&Rect, WS_OVERLAPPEDWINDOW, FALSE);

    WindowHandle = CreateWindowEx(0, ClassName, L"Rhythm Jungle", WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT, Rect.right - Rect.left,
                                  Rect.bottom - Rect.top, nullptr, nullptr, InstanceHandle, this);

    if (WindowHandle == nullptr)
        return false;

    ShowWindow(WindowHandle, SW_SHOW);
    UpdateWindow(WindowHandle);

    return true;
}

void FWindow::Shutdown()
{
    if (WindowHandle != nullptr)
    {
        DestroyWindow(WindowHandle);
        WindowHandle = nullptr;
    }

    if (InstanceHandle != nullptr)
    {
        UnregisterClass(L"MyWindowClass", InstanceHandle);
    }
}

bool FWindow::ProcessMessages()
{
    MSG Msg{};

    while (PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);

        if (Msg.message == WM_QUIT)
        {
            bShouldClose = true;
            return false;
        }
    }

    return !bShouldClose;
}

HWND FWindow::GetHandle() const { return WindowHandle; }

LRESULT CALLBACK FWindow::WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    FWindow *Window = nullptr;

    if (Msg == WM_NCCREATE)
    {
        CREATESTRUCT *CreateStruct = reinterpret_cast<CREATESTRUCT *>(lParam);
        Window = reinterpret_cast<FWindow *>(CreateStruct->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(Window));
    }
    else
    {
        Window = reinterpret_cast<FWindow *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (Window != nullptr)
    {
        return Window->HandleMessage(hWnd, Msg, wParam, lParam);
    }

    return DefWindowProc(hWnd, Msg, wParam, lParam);
}

LRESULT FWindow::HandleMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam))
        return true;

    switch (Msg)
    {
    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, Msg, wParam, lParam);
}
