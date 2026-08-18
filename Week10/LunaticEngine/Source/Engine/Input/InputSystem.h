#pragma once

#include "Core/CoreTypes.h"

enum class EInputEventType : unsigned char
{
	KeyDown,
	KeyUp,
	MouseButtonDown,
	MouseButtonUp,
	MouseWheel,
};

struct FInputEvent
{
	EInputEventType Type = EInputEventType::KeyDown;
	int32 KeyOrButton = 0;
	float Value = 0.0f;
};

struct FInputPoint
{
	long x = 0;
	long y = 0;
};

struct FInputSystemSnapshot
{
	static constexpr int32 MaxKeys = 256;

	TStaticArray<bool, MaxKeys> KeyState{};
	TStaticArray<bool, MaxKeys> PrevKeyState{};
	TStaticArray<bool, MaxKeys> DraggingState{};
	TStaticArray<bool, MaxKeys> DragStartedState{};
	TStaticArray<bool, MaxKeys> DragEndedState{};
	TStaticArray<FInputPoint, MaxKeys> DragDelta{};

	TArray<FInputEvent> EventQueue;

	FInputPoint MousePosition{};
	float MouseDeltaX = 0.0f;
	float MouseDeltaY = 0.0f;
	float MouseWheelDelta = 0.0f;

	bool bWindowFocused = true;
	bool bGuiUsingMouse = false;
	bool bGuiUsingKeyboard = false;
	bool bGuiUsingTextInput = false;

	bool IsKeyDown(int32 Key) const;
	bool IsKeyPressed(int32 Key) const;
	bool IsKeyReleased(int32 Key) const;

	bool IsMouseButtonDown(int32 Button) const { return IsKeyDown(Button); }
	bool IsMouseButtonPressed(int32 Button) const { return IsKeyPressed(Button); }
	bool IsMouseButtonReleased(int32 Button) const { return IsKeyReleased(Button); }

	bool IsDragging(int32 Button) const;
	bool WasDragStarted(int32 Button) const;
	bool WasDragEnded(int32 Button) const;
	FInputPoint GetDragDelta(int32 Button) const;

	bool IsWindowFocused() const { return bWindowFocused; }
	bool IsGuiUsingMouse() const { return bGuiUsingMouse; }
	bool IsGuiUsingKeyboard() const { return bGuiUsingKeyboard || bGuiUsingTextInput; }
	bool IsGuiUsingTextInput() const { return bGuiUsingTextInput; }

	float GetMouseDeltaX() const { return MouseDeltaX; }
	float GetMouseDeltaY() const { return MouseDeltaY; }
	float GetMouseWheelDelta() const { return MouseWheelDelta; }
	bool MouseMoved() const;

	bool HasMouseInput() const;
	bool HasKeyboardInput() const;
};

class FInputSystem
{
public:
	static FInputSystemSnapshot MakeSnapshot();
};
