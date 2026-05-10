#include "Input/InputRouter.h"

#include "Input/InputSystem.h"
#include "Viewport/ViewportClient.h"

FInputRouter& FInputRouter::Get()
{
	static FInputRouter Instance;
	return Instance;
}

// Main Snapshot Router: 캡쳐된 뷰포트 - UI - 호버링 중인 뷰포트 순으로 입력을 라우팅한다. 
bool FInputRouter::RouteSnapshot(const FInputSystemSnapshot& Snapshot, float DeltaTime)
{
	if (MouseCapturedViewport)
	{
		return MouseCapturedViewport->HandleInputSnapshot(Snapshot, DeltaTime);
	}

	if (DoesUILayerConsumeInput(Snapshot))
	{
		return false;
	}

	FViewportClient* TargetViewport = ResolveTargetViewport();
	if (!TargetViewport)
	{
		return false;
	}

	return TargetViewport->HandleInputSnapshot(Snapshot, DeltaTime);
}

void FInputRouter::SetUILayerConsumer(FInputUILayerConsumer InConsumer)
{
	UILayerConsumer = std::move(InConsumer);
}

void FInputRouter::ClearUILayerConsumer()
{
	UILayerConsumer = nullptr;
}

void FInputRouter::SetMouseCapturedViewport(FViewportClient* InViewport)
{
	if (bPIEMode)
	{
		SavedMouseCapturedViewport = InViewport;
		return;
	}
	MouseCapturedViewport = InViewport;
}

void FInputRouter::ReleaseMouseCapture(FViewportClient* InViewport)
{
	if (bPIEMode)
	{
		if (!InViewport || SavedMouseCapturedViewport == InViewport)
		{
			SavedMouseCapturedViewport = nullptr;
		}
		return;
	}

	if (!InViewport || MouseCapturedViewport == InViewport)
	{
		MouseCapturedViewport = nullptr;
	}
}

void FInputRouter::SetKeyboardFocusedViewport(FViewportClient* InViewport)
{
	if (bPIEMode)
	{
		SavedKeyboardFocusedViewport = InViewport;
		return;
	}
	KeyboardFocusedViewport = InViewport;
}

void FInputRouter::SetHoveredViewport(FViewportClient* InViewport)
{
	if (bPIEMode)
	{
		SavedHoveredViewport = InViewport;
		return;
	}
	HoveredViewport = InViewport;
}

void FInputRouter::ClearViewport(FViewportClient* InViewport)
{
	if (!InViewport)
	{
		return;
	}

	if (MouseCapturedViewport == InViewport) MouseCapturedViewport = nullptr;
	if (KeyboardFocusedViewport == InViewport) KeyboardFocusedViewport = nullptr;
	if (HoveredViewport == InViewport) HoveredViewport = nullptr;
	if (PIEViewport == InViewport) PIEViewport = nullptr;
	if (SavedMouseCapturedViewport == InViewport) SavedMouseCapturedViewport = nullptr;
	if (SavedKeyboardFocusedViewport == InViewport) SavedKeyboardFocusedViewport = nullptr;
	if (SavedHoveredViewport == InViewport) SavedHoveredViewport = nullptr;
}

void FInputRouter::BeginPIEMode(FViewportClient* InGameViewport)
{
	if (!InGameViewport)
	{
		return;
	}

	if (!bPIEMode)
	{
		SavedMouseCapturedViewport = MouseCapturedViewport;
		SavedKeyboardFocusedViewport = KeyboardFocusedViewport;
		SavedHoveredViewport = HoveredViewport;
	}

	bPIEMode = true;
	PIEViewport = InGameViewport;
	MouseCapturedViewport = nullptr;
	KeyboardFocusedViewport = InGameViewport;
	HoveredViewport = InGameViewport;
}

void FInputRouter::EndPIEMode()
{
	if (!bPIEMode)
	{
		return;
	}

	bPIEMode = false;
	PIEViewport = nullptr;
	MouseCapturedViewport = SavedMouseCapturedViewport;
	KeyboardFocusedViewport = SavedKeyboardFocusedViewport;
	HoveredViewport = SavedHoveredViewport;
	SavedMouseCapturedViewport = nullptr;
	SavedKeyboardFocusedViewport = nullptr;
	SavedHoveredViewport = nullptr;
}

// UI Layer에게 먼저 입력을 전송하고, 소비하지 않았을 경우에 Keyboard, Mouse가 입력을 소비한다.
bool FInputRouter::DoesUILayerConsumeInput(const FInputSystemSnapshot& Snapshot) const
{
	if (UILayerConsumer)
	{
		return UILayerConsumer(Snapshot);
	}

	const bool bMouseConsumed = Snapshot.IsGuiUsingMouse() && Snapshot.HasMouseInput();
	const bool bKeyboardConsumed = Snapshot.IsGuiUsingKeyboard() && Snapshot.HasKeyboardInput();
	return bMouseConsumed || bKeyboardConsumed;
}

FViewportClient* FInputRouter::ResolveTargetViewport() const
{
	if (MouseCapturedViewport)
	{
		return MouseCapturedViewport;
	}

	if (KeyboardFocusedViewport)
	{
		return KeyboardFocusedViewport;
	}

	return HoveredViewport;
}
