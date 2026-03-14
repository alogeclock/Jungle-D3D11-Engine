#include "Engine/Source/Runtime/Editor/Public/Viewport.h"

FViewport::FViewport()
	: ViewportClient(nullptr)
{
}

FViewport::~FViewport()
{
	delete ViewportClient;
}

void FViewport::CreateEditorViewportClient()
{
	ViewportClient = new FEditorViewportClient();
}

void FViewport::OnKeyDown(uint32 KeyCode)
{
	KeyStates[KeyCode] = true;
	if (ViewportClient)
	{
		FInputEventState State(this, FKey(KeyCode), EInputEvent::Pressed);
		ViewportClient->InputKey(State);
	}
}

void FViewport::OnKeyUp(uint32 KeyCode)
{
	KeyStates[KeyCode] = false;
	if (ViewportClient)
	{
		FInputEventState State(this, FKey(KeyCode), EInputEvent::Released);
		ViewportClient->InputKey(State);
	}
}

void FViewport::OnMouseMove(int32 X, int32 Y)
{
	MouseDeltaX = X - PrevMouseX;
	MouseDeltaY = Y - PrevMouseY;
	PrevMouseX = MouseX; PrevMouseY = MouseY;
	MouseX = X;  MouseY = Y;

	if (ViewportClient)
		ViewportClient->MouseMove(this, X, Y);
}

void FViewport::OnMouseButtonDown(uint32 KeyCode, int32 X, int32 Y)
{
	KeyStates[KeyCode] = true;
	MouseX = X; MouseY = Y;
	if (ViewportClient)
	{
		FInputEventState State(this, FKey(KeyCode), EInputEvent::Pressed);
		ViewportClient->InputKey(State);
	}
}

void FViewport::OnMouseButtonUp(uint32 KeyCode)
{
	KeyStates[KeyCode] = false;
	if (ViewportClient)
	{
		FInputEventState State(this, FKey(KeyCode), EInputEvent::Released);
		ViewportClient->InputKey(State);
	}
}

void FViewport::OnMouseWheel(float Delta)
{
	if (ViewportClient)
		ViewportClient->InputAxis(this, FKey(0), Delta, 0.016f);
}

bool FViewport::KeyState(FKey InKey) const
{
	auto It = KeyStates.find(InKey.KeyCode);
	return It != KeyStates.end() && It->second;
}

void FViewport::Tick(float DeltaTime)
{
	if (ViewportClient)
		ViewportClient->Tick(DeltaTime, this);
}
