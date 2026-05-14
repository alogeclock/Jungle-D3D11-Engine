#include "Engine/Runtime/WindowsApplication.h"
#include "Engine/Runtime/resource.h"
#include "Core/ProjectSettings.h"
#include "Engine/Input/InputManager.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_internal.h"

#include <algorithm>
#include <optional>
#include <windowsx.h>

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, unsigned int Msg, WPARAM wParam, LPARAM lParam);

namespace
{
	// Editor Mode
	bool IsEditorMode();
	LONG GetApplicationWindowStyle();
	
	// Window Size 초기화 및 보정 처리
	void GetInitialWindowBounds(int& OutX, int& OutY, int& OutWidth, int& OutHeight);
	int GetResizeBorderForWindow(HWND hWnd);
	void ApplyMaximizedWindowBounds(HWND hWnd, LPARAM lParam);
	bool ShouldForwardThreadMessageToInputManager(HWND MessageHwnd, HWND MainHwnd);
	
	// 입력 처리 및 Hit Test
	LRESULT HitTestEditorFrame(HWND hWnd, const FWindowsWindow& Window, LPARAM lParam);
	bool IsImGuiWindowBlockingTitleBarDrag(HWND hWnd, POINT ScreenPoint);
	void RegisterRawMouseInput(HWND hWnd);	
}

// HWND에 연결된 FWindowsApplication 인스턴스를 찾아 멤버 WndProc로 메시지를 전달한다.
LRESULT CALLBACK FWindowsApplication::StaticWndProc(HWND hWnd, unsigned int Msg, WPARAM wParam, LPARAM lParam)
{
	FWindowsApplication* App = reinterpret_cast<FWindowsApplication*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	if (Msg == WM_NCCREATE)
	{
		CREATESTRUCT* CreateStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
		App = reinterpret_cast<FWindowsApplication*>(CreateStruct->lpCreateParams);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(App));
	}

	if (App)
	{
		return App->WndProc(hWnd, Msg, wParam, lParam);
	}

	return DefWindowProc(hWnd, Msg, wParam, lParam);
}

// Win32 메시지를 ImGui, 입력 시스템, 애플리케이션별 처리 흐름으로 분배한다.
LRESULT FWindowsApplication::WndProc(HWND hWnd, unsigned int Msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam))
		return TRUE;

	FInputManager::Get().ProcessMessage(hWnd, Msg, wParam, lParam);

	if (auto Result = HandleLifecycleMessage(hWnd, Msg, wParam, lParam))
		return *Result;

	if (auto Result = HandleNonClientMessage(hWnd, Msg, wParam, lParam))
		return *Result;

	if (auto Result = HandleResizeMessage(hWnd, Msg, wParam, lParam))
		return *Result;

	return DefWindowProc(hWnd, Msg, wParam, lParam);
}

// 창 종료처럼 애플리케이션 생명주기에 직접 연결된 메시지를 처리한다.
std::optional<LRESULT> FWindowsApplication::HandleLifecycleMessage(HWND hWnd, unsigned int Msg, WPARAM wParam, LPARAM lParam)
{
	if (Msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}
	return std::nullopt;
}

// 커스텀 프레임, 최대화 영역 등 클라이언트가 아닌 영역의 메시지를 처리한다.
std::optional<LRESULT> FWindowsApplication::HandleNonClientMessage(HWND hWnd, unsigned int Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
	case WM_NCCALCSIZE:
		if (IsEditorMode()) return 0;
		break;
	case WM_GETMINMAXINFO:
		ApplyMaximizedWindowBounds(hWnd, lParam);
		return 0;
	case WM_NCHITTEST:
		if (IsEditorMode())
			return HitTestEditorFrame(hWnd, Window, lParam);
		break;
	}
	return std::nullopt;
}

// 리사이즈와 리사이즈 중 갱신에 관련된 메시지를 처리한다.
std::optional<LRESULT> FWindowsApplication::HandleResizeMessage(HWND hWnd, unsigned int Msg, WPARAM wParam, LPARAM lParam)
{
	constexpr UINT ResizeTimerId = 1;
	constexpr UINT ResizeIntervalMs = 16;

	switch (Msg)
	{
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED)
		{
			const unsigned int Width = LOWORD(lParam);
			const unsigned int Height = HIWORD(lParam);
			Window.OnResized(Width, Height);
			
			if (OnResizedCallback)
			{
				OnResizedCallback(Width, Height);
			}
		}
		return 0;

	case WM_ENTERSIZEMOVE:
		bIsResizing = true;
		SetTimer(hWnd, ResizeTimerId, ResizeIntervalMs, nullptr);
		return 0;

	case WM_EXITSIZEMOVE:
		bIsResizing = false;
		KillTimer(hWnd, ResizeTimerId);
		return 0;

	case WM_SIZING:
		RedrawDuringResize(hWnd);
		return 0;

	case WM_TIMER:
		if (wParam == ResizeTimerId && bIsResizing && OnSizingCallback)
		{
			RedrawDuringResize(hWnd);
			return 0;
		}
		break;
	}
	return std::nullopt;
}

// 리사이즈 중에도 엔진 틱과 윈도우 갱신을 수행한다.
void FWindowsApplication::RedrawDuringResize(HWND hWnd)
{
	if (OnSizingCallback)
	{
		OnSizingCallback();
	}
	RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

// Win32 윈도우 클래스를 등록하고 메인 윈도우를 생성한다.
bool FWindowsApplication::Init(HINSTANCE InHInstance)
{
	HInstance = InHInstance;
	ImGui_ImplWin32_EnableDpiAwareness();

	WNDCLASSEXW WndClass{};
	WndClass.cbSize = sizeof(WNDCLASSEXW);
	WndClass.lpfnWndProc = StaticWndProc;
	WndClass.hInstance = HInstance;
	WndClass.hIcon = LoadIconW(HInstance, MAKEINTRESOURCEW(IDI_APP_ICON));
	WndClass.hIconSm = WndClass.hIcon;
	WndClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	WndClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
	WndClass.lpszClassName = L"JungleWindowClass";

	if (!RegisterClassExW(&WndClass))
		return false;

	int InitX = 0, InitY = 0, InitWidth = 0, InitHeight = 0;
	GetInitialWindowBounds(InitX, InitY, InitWidth, InitHeight);

	HWND HWindow = CreateWindowExW(
		0, WndClass.lpszClassName, L"LunaticEngine", GetApplicationWindowStyle(),
		InitX, InitY, InitWidth, InitHeight,
		nullptr, nullptr, HInstance, this
	);

	if (!HWindow)
		return false;

	RegisterRawMouseInput(HWindow);
	Window.Initialize(HWindow);
	
	if (IsEditorMode())
	{
		Window.SetResizeLocked(false);
		Window.ResizeClientArea(static_cast<unsigned int>(InitWidth), static_cast<unsigned int>(InitHeight));
	}
	else
	{
		SetWindowPos(HWindow, HWND_TOP, InitX, InitY, InitWidth, InitHeight, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
	}

	return true;
}

// 큐에 쌓인 Win32 메시지를 처리하고 종료 요청 상태를 갱신한다.
void FWindowsApplication::PumpMessages()
{
	MSG Msg;
	while (PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (ShouldForwardThreadMessageToInputManager(Msg.hwnd, Window.GetHWND()))
		{
			FInputManager::Get().ProcessMessage(Msg.hwnd, Msg.message, Msg.wParam, Msg.lParam);
		}

		TranslateMessage(&Msg);
		DispatchMessage(&Msg);

		if (Msg.message == WM_QUIT)
		{
			bIsExitRequested = true;
			break;
		}
	}
}

// 생성된 메인 윈도우를 파괴한다.
void FWindowsApplication::Destroy()
{
	if (Window.GetHWND())
	{
		DestroyWindow(Window.GetHWND());
	}
}

namespace
{
	// 현재 빌드가 에디터형 윈도우 구성을 사용하는지 반환한다.
	bool IsEditorMode()
	{
		#if WITH_EDITOR || IS_OBJ_VIEWER
			return true;
		#else
			return false;
		#endif
	}

	// 현재 애플리케이션 모드에 맞는 Win32 윈도우 스타일을 반환한다.
	LONG GetApplicationWindowStyle()
	{
		return IsEditorMode() ? (WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_VISIBLE) : (WS_POPUP | WS_VISIBLE);
	}

	// 현재 애플리케이션 모드에 맞는 초기 윈도우 위치와 크기를 계산한다.
	void GetInitialWindowBounds(int& OutX, int& OutY, int& OutWidth, int& OutHeight)
	{
		if (IsEditorMode())
		{
			OutX = CW_USEDEFAULT;
			OutY = CW_USEDEFAULT;
			OutWidth = static_cast<int>((std::max)(320u, FProjectSettings::Get().Game.WindowWidth));
			OutHeight = static_cast<int>((std::max)(240u, FProjectSettings::Get().Game.WindowHeight));
		}
		else
		{
			RECT Rect{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
			MONITORINFO MonitorInfo{ sizeof(MONITORINFO) };
			if (GetMonitorInfoW(MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), &MonitorInfo))
			{
				Rect = MonitorInfo.rcMonitor;
			}

			OutX = Rect.left;
			OutY = Rect.top;
			OutWidth = static_cast<int>((std::max)(1L, Rect.right - Rect.left));
			OutHeight = static_cast<int>((std::max)(1L, Rect.bottom - Rect.top));
		}
	}

	// 현재 DPI에 맞는 리사이즈 테두리 두께를 계산한다.
	int GetResizeBorderForWindow(HWND hWnd)
	{
		if (!hWnd) return 0;

		const UINT Dpi = GetDpiForWindow(hWnd);
		return GetSystemMetricsForDpi(SM_CXFRAME, Dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, Dpi);
	}

	// 최대화 시 작업 영역을 기준으로 윈도우 크기 제한 정보를 보정한다.
	void ApplyMaximizedWindowBounds(HWND hWnd, LPARAM lParam)
	{
		MONITORINFO MonitorInfo{ sizeof(MONITORINFO) };
		if (!GetMonitorInfoW(MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), &MonitorInfo))
			return;

		MINMAXINFO* MinMaxInfo = reinterpret_cast<MINMAXINFO*>(lParam);
		const RECT& WorkArea = MonitorInfo.rcWork;
		const RECT& MonitorArea = MonitorInfo.rcMonitor;

		MinMaxInfo->ptMaxPosition.x = WorkArea.left - MonitorArea.left;
		MinMaxInfo->ptMaxPosition.y = WorkArea.top - MonitorArea.top;
		MinMaxInfo->ptMaxSize.x = WorkArea.right - WorkArea.left;
		MinMaxInfo->ptMaxSize.y = WorkArea.bottom - WorkArea.top;
	}

	bool ShouldForwardThreadMessageToInputManager(HWND MessageHwnd, HWND MainHwnd)
	{
		if (!MessageHwnd || MessageHwnd == MainHwnd)
		{
			return false;
		}

		DWORD ProcessId = 0;
		GetWindowThreadProcessId(MessageHwnd, &ProcessId);
		return ProcessId == GetCurrentProcessId();
	}

	// 에디터 창에서 마우스 위치에 맞는 히트 결과를 반환한다.
	LRESULT HitTestEditorFrame(HWND hWnd, const FWindowsWindow& Window, LPARAM lParam)
	{
		POINT Cursor = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		POINT ClientPoint = Cursor;
		ScreenToClient(hWnd, &ClientPoint);

		if (Window.IsInTitleBarControlRegion(ClientPoint))
			return HTCLIENT;

		RECT WindowRect{};
		GetWindowRect(hWnd, &WindowRect);
		
		const int ResizeBorderThickness = GetResizeBorderForWindow(hWnd);
		const bool bAllowResize = !Window.IsResizeLocked() && !IsZoomed(hWnd) && (ResizeBorderThickness > 0);

		if (bAllowResize)
		{
			const bool bLeft = (Cursor.x >= WindowRect.left) && (Cursor.x < WindowRect.left + ResizeBorderThickness);
			const bool bRight = (Cursor.x < WindowRect.right) && (Cursor.x >= WindowRect.right - ResizeBorderThickness);
			const bool bTop = (Cursor.y >= WindowRect.top) && (Cursor.y < WindowRect.top + ResizeBorderThickness);
			const bool bBottom = (Cursor.y < WindowRect.bottom) && (Cursor.y >= WindowRect.bottom - ResizeBorderThickness);

			if (bTop && bLeft) return HTTOPLEFT;
			if (bTop && bRight) return HTTOPRIGHT;
			if (bBottom && bLeft) return HTBOTTOMLEFT;
			if (bBottom && bRight) return HTBOTTOMRIGHT;
			if (bTop) return HTTOP;
			if (bLeft) return HTLEFT;
			if (bRight) return HTRIGHT;
			if (bBottom) return HTBOTTOM;
		}

		if (Window.IsInTitleBarDragRegion(ClientPoint))
		{
			if (IsImGuiWindowBlockingTitleBarDrag(hWnd, Cursor))
			{
				return HTCLIENT;
			}
			return HTCAPTION;
		}

		return HTCLIENT;
	}

	bool IsImGuiWindowBlockingTitleBarDrag(HWND hWnd, POINT ScreenPoint)
	{
		if (!GImGui)
		{
			return false;
		}

		ImGuiContext& Context = *GImGui;
		const ImVec2 Point(static_cast<float>(ScreenPoint.x), static_cast<float>(ScreenPoint.y));
		for (int32 Index = Context.Windows.Size - 1; Index >= 0; --Index)
		{
			ImGuiWindow* Window = Context.Windows[Index];
			if (!Window || !Window->WasActive || Window->Hidden)
			{
				continue;
			}

			if ((Window->Flags & ImGuiWindowFlags_NoInputs) != 0)
			{
				continue;
			}

			if (Window->Viewport && Window->Viewport->PlatformHandle && Window->Viewport->PlatformHandle != hWnd)
			{
				continue;
			}

			const ImRect Rect = Window->OuterRectClipped;
			if (Point.x >= Rect.Min.x && Point.x < Rect.Max.x && Point.y >= Rect.Min.y && Point.y < Rect.Max.y)
			{
				return true;
			}
		}

		return false;
	}

	// 윈도우 포커스와 관계없이 마우스 RawInput을 받을 수 있도록 등록한다.
	void RegisterRawMouseInput(HWND hWnd)
	{
		RAWINPUTDEVICE RawMouseDevice{};
		RawMouseDevice.usUsagePage = 0x01; // HID_USAGE_PAGE_GENERIC
		RawMouseDevice.usUsage = 0x02; // HID_USAGE_GENERIC_MOUSE
		RawMouseDevice.dwFlags = RIDEV_INPUTSINK;
		RawMouseDevice.hwndTarget = hWnd;
		
		RegisterRawInputDevices(&RawMouseDevice, 1, sizeof(RAWINPUTDEVICE));
	}
}
