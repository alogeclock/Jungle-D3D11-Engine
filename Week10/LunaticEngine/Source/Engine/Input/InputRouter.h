#pragma once

#include "Core/CoreTypes.h"

#include <functional>

class FViewportClient;
struct FInputSystemSnapshot;

using FInputUILayerConsumer = std::function<bool(const FInputSystemSnapshot&)>;

class FInputRouter
{
public:
	static FInputRouter& Get();

	// Main Routing
	bool RouteSnapshot(const FInputSystemSnapshot& Snapshot, float DeltaTime);

	// Initialize & Reset
	void SetUILayerConsumer(FInputUILayerConsumer InConsumer);
	void ClearUILayerConsumer();

	// Mouse, KeyBoard Capture & Hovering (Viewport)
	void SetMouseCapturedViewport(FViewportClient* InViewport);
	void ReleaseMouseCapture(FViewportClient* InViewport);
	void SetKeyboardFocusedViewport(FViewportClient* InViewport);
	void SetHoveredViewport(FViewportClient* InViewport);
	void ClearViewport(FViewportClient* InViewport);

	// PIE
	void BeginPIEMode(FViewportClient* InGameViewport);
	void EndPIEMode();
	bool IsPIEMode() const { return bPIEMode; }

	// Getter
	FViewportClient* GetMouseCapturedViewport() const { return MouseCapturedViewport; }
	FViewportClient* GetKeyboardFocusedViewport() const { return KeyboardFocusedViewport; }
	FViewportClient* GetHoveredViewport() const { return HoveredViewport; }

private:
	FInputRouter() = default;

	// UI & Viewport
	bool DoesUILayerConsumeInput(const FInputSystemSnapshot& Snapshot) const;
	FViewportClient* ResolveTargetViewport() const;

	FInputUILayerConsumer UILayerConsumer;

	FViewportClient* MouseCapturedViewport = nullptr;
	FViewportClient* KeyboardFocusedViewport = nullptr;
	FViewportClient* HoveredViewport = nullptr;

	bool bPIEMode = false;
	FViewportClient* PIEViewport = nullptr;
	FViewportClient* SavedMouseCapturedViewport = nullptr;
	FViewportClient* SavedKeyboardFocusedViewport = nullptr;
	FViewportClient* SavedHoveredViewport = nullptr;
};
