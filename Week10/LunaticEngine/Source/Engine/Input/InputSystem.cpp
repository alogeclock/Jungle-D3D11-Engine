#include "Input/InputSystem.h"

#include "Input/InputManager.h"

#include <cmath>

namespace
{
	bool IsValidKey(int32 Key)
	{
		return Key >= 0 && Key < FInputSystemSnapshot::MaxKeys;
	}

	bool IsMouseButtonKey(int32 Key)
	{
		return Key == 0x01 || Key == 0x02 || Key == 0x04 || Key == 0x05 || Key == 0x06;
	}

	bool IsMouseEvent(EInputEventType Type)
	{
		return Type == EInputEventType::MouseButtonDown ||
			Type == EInputEventType::MouseButtonUp ||
			Type == EInputEventType::MouseWheel;
	}
}

bool FInputSystemSnapshot::IsKeyDown(int32 Key) const
{
	return IsValidKey(Key) && KeyState[Key];
}

bool FInputSystemSnapshot::IsKeyPressed(int32 Key) const
{
	return IsValidKey(Key) && KeyState[Key] && !PrevKeyState[Key];
}

bool FInputSystemSnapshot::IsKeyReleased(int32 Key) const
{
	return IsValidKey(Key) && !KeyState[Key] && PrevKeyState[Key];
}

bool FInputSystemSnapshot::IsDragging(int32 Button) const
{
	return IsValidKey(Button) && DraggingState[Button];
}

bool FInputSystemSnapshot::WasDragStarted(int32 Button) const
{
	return IsValidKey(Button) && DragStartedState[Button];
}

bool FInputSystemSnapshot::WasDragEnded(int32 Button) const
{
	return IsValidKey(Button) && DragEndedState[Button];
}

FInputPoint FInputSystemSnapshot::GetDragDelta(int32 Button) const
{
	if (!IsValidKey(Button))
	{
		return {};
	}
	return DragDelta[Button];
}

bool FInputSystemSnapshot::MouseMoved() const
{
	return std::abs(MouseDeltaX) > 1e-6f || std::abs(MouseDeltaY) > 1e-6f;
}

bool FInputSystemSnapshot::HasMouseInput() const
{
	if (MouseMoved() || std::abs(MouseWheelDelta) > 1e-6f)
	{
		return true;
	}

	for (const FInputEvent& Event : EventQueue)
	{
		if (IsMouseEvent(Event.Type))
		{
			return true;
		}
	}

	for (int32 Key = 0; Key < MaxKeys; ++Key)
	{
		if (IsMouseButtonKey(Key) && (KeyState[Key] || PrevKeyState[Key] || DraggingState[Key]))
		{
			return true;
		}
	}

	return false;
}

bool FInputSystemSnapshot::HasKeyboardInput() const
{
	for (const FInputEvent& Event : EventQueue)
	{
		if (Event.Type == EInputEventType::KeyDown || Event.Type == EInputEventType::KeyUp)
		{
			return true;
		}
	}

	for (int32 Key = 0; Key < MaxKeys; ++Key)
	{
		if (!IsMouseButtonKey(Key) && KeyState[Key])
		{
			return true;
		}
	}

	return false;
}

FInputSystemSnapshot FInputSystem::MakeSnapshot()
{
	FInputManager& Input = FInputManager::Get();
	FInputSystemSnapshot Snapshot;

	Snapshot.EventQueue = Input.GetFrameEventQueue();
	Snapshot.MouseDeltaX = Input.GetMouseDeltaX();
	Snapshot.MouseDeltaY = Input.GetMouseDeltaY();
	Snapshot.MouseWheelDelta = Input.GetMouseWheelDelta();
	Snapshot.bWindowFocused = Input.IsWindowFocused();
	Snapshot.bGuiUsingMouse = Input.IsGuiUsingMouse();
	Snapshot.bGuiUsingKeyboard = Input.IsGuiUsingKeyboard();
	Snapshot.bGuiUsingTextInput = Input.IsGuiUsingTextInput();

	POINT MousePos = Input.GetMousePos();
	Snapshot.MousePosition = { MousePos.x, MousePos.y };

	for (int32 Key = 0; Key < FInputSystemSnapshot::MaxKeys; ++Key)
	{
		Snapshot.KeyState[Key] = Input.IsKeyDown(Key);
		Snapshot.PrevKeyState[Key] = Input.WasKeyDown(Key);
		Snapshot.DraggingState[Key] = Input.IsDragging(Key);
		Snapshot.DragStartedState[Key] = Input.WasDragStarted(Key);
		Snapshot.DragEndedState[Key] = Input.WasDragEnded(Key);

		POINT Delta = Input.GetDragDelta(Key);
		Snapshot.DragDelta[Key] = { Delta.x, Delta.y };
	}

	return Snapshot;
}
